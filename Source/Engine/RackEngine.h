#pragma once
// Bub Leon Mix Rack — slot chain with lock-free module swaps, oversampling and latency-aligned dry path
#include <juce_dsp/juce_dsp.h>
#include <mutex>
#include "Engine/ModuleRegistry.h"

namespace bl
{
/** A fully prepared module ready to be handed to the audio thread. */
struct LoadedModule
{
    std::unique_ptr<ModuleBase> module;
    int type = -1;
    int osIndex = 0;      // 0 = off, 1 = 2x, 2 = 4x
    int latency = 0;      // base-rate samples, includes oversampling filters
};

/** Pointers to the slot's parameters (owned by the APVTS). */
struct SlotParamRefs
{
    std::atomic<float>* norm[kParamsPerSlot] {};
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* solo = nullptr;
    std::atomic<float>* mix = nullptr;     // 0..100 %
    std::atomic<float>* inTrim = nullptr;  // dB
    std::atomic<float>* outTrim = nullptr; // dB
};

class RackSlot
{
public:
    ~RackSlot()
    {
        delete current.load();
        delete pending.exchange (nullptr);
        delete trash.exchange (nullptr);
    }

    void setParamRefs (const SlotParamRefs& r) { refs = r; }

    void prepare (double newFs, int newMaxBlock)
    {
        fs = newFs; maxBlock = newMaxBlock;
        using OS = juce::dsp::Oversampling<double>;
        for (int k = 0; k < 2; ++k)
        {
            os[k] = std::make_unique<OS> (2, (size_t) (k + 1), OS::filterHalfBandFIREquiripple, true, true);
            os[k]->initProcessing ((size_t) maxBlock);
        }
        dry[0].assign ((size_t) maxBlock, 0.0); dry[1].assign ((size_t) maxBlock, 0.0);
        dryDelay.prepare (kMaxLatency + 4);
        bypassS.reset (fs, 0.012); mixS.reset (fs, 0.03); inS.reset (fs, 0.03); outS.reset (fs, 0.03);
        bypassS.setCurrentAndTargetValue (refs.bypass && refs.bypass->load() > 0.5f ? 1.0 : 0.0);
        mixS.setCurrentAndTargetValue (refs.mix ? refs.mix->load() * 0.01 : 1.0);
        inS.setCurrentAndTargetValue (refs.inTrim ? juce::Decibels::decibelsToGain ((double) refs.inTrim->load()) : 1.0);
        outS.setCurrentAndTargetValue (refs.outTrim ? juce::Decibels::decibelsToGain ((double) refs.outTrim->load()) : 1.0);
    }

    int oversamplingLatency (int osIndex) const
    {
        return (osIndex > 0 && os[osIndex - 1]) ? (int) std::lround (os[osIndex - 1]->getLatencyInSamples()) : 0;
    }

    /** Message thread: build a prepared module for the current sample rate. */
    std::unique_ptr<LoadedModule> build (int type, int osIndex) const
    {
        auto lm = std::make_unique<LoadedModule>();
        lm->type = type;
        lm->module = ModuleRegistry::create (type);
        if (lm->module == nullptr) { lm->type = -1; return lm; }
        if (! lm->module->info().supportsOversampling) osIndex = 0;
        lm->osIndex = osIndex;
        const int factor = 1 << osIndex;
        lm->module->prepare (fs * factor, maxBlock * factor, factor);
        lm->module->reset();
        lm->latency = juce::jmin (kMaxLatency, oversamplingLatency (osIndex) + lm->module->latency() / factor);
        return lm;
    }

    /** Message thread: queue a module for the audio thread. */
    void install (std::unique_ptr<LoadedModule> lm)
    {
        installedType.store (lm->type);
        installedOs.store (lm->osIndex);
        installedLatency.store (lm->latency);
        delete pending.exchange (lm.release());
    }

    /** Audio stopped (prepareToPlay / constructor): adopt the pending module immediately. */
    void adoptPendingNow()
    {
        if (auto* p = pending.exchange (nullptr))
        {
            delete current.exchange (p);
            resetState();
        }
        delete trash.exchange (nullptr);
    }

    /** Message thread: free modules the audio thread has retired. */
    void collectGarbage() { delete trash.exchange (nullptr); }

    int getInstalledType() const noexcept     { return installedType.load(); }
    int getInstalledOs() const noexcept       { return installedOs.load(); }
    int getInstalledLatency() const noexcept  { return installedLatency.load(); }

    float takePeak() noexcept       { return outPeak.exchange (0.0f); }
    float getGrDb() const noexcept  { return gr.load(); }

    //==========================================================================
    /** Audio thread. */
    void process (double* l, double* r, int n, bool soloActive) noexcept
    {
        swapIfPending();
        LoadedModule* lm = current.load (std::memory_order_acquire);
        if (lm == nullptr || lm->module == nullptr) { meter (l, r, n, 0.0f); return; }

        const int lat = lm->latency;
        for (int i = 0; i < n; ++i)
        {
            if (lat > 0)
            {
                dryDelay.push (l[i], r[i]);
                dry[0][(size_t) i] = dryDelay.read (0, lat + 1);
                dry[1][(size_t) i] = dryDelay.read (1, lat + 1);
            }
            else { dry[0][(size_t) i] = l[i]; dry[1][(size_t) i] = r[i]; }
        }

        const bool byp = refs.bypass->load() > 0.5f || (soloActive && refs.solo->load() < 0.5f);
        bypassS.setTargetValue (byp ? 1.0 : 0.0);
        mixS.setTargetValue (juce::jlimit (0.0, 1.0, refs.mix->load() * 0.01));
        inS.setTargetValue (juce::Decibels::decibelsToGain ((double) refs.inTrim->load(), -120.0));
        outS.setTargetValue (juce::Decibels::decibelsToGain ((double) refs.outTrim->load(), -120.0));

        if (! bypassS.isSmoothing() && bypassS.getCurrentValue() >= 1.0)
        {
            std::copy (dry[0].begin(), dry[0].begin() + n, l);
            std::copy (dry[1].begin(), dry[1].begin() + n, r);
            gr.store (0.0f);
            meter (l, r, n, 0.0f);
            return;
        }

        const auto& specs = lm->module->info().params;
        double real[kParamsPerSlot] {};
        for (size_t k = 0; k < specs.size() && k < (size_t) kParamsPerSlot; ++k)
            real[k] = specs[k].toReal ((double) refs.norm[k]->load());
        lm->module->setParams (real);

        for (int i = 0; i < n; ++i) { const double g = inS.getNextValue(); l[i] *= g; r[i] *= g; }

        if (lm->osIndex > 0)
        {
            double* chans[2] = { l, r };
            juce::dsp::AudioBlock<double> block (chans, 2, (size_t) n);
            auto& o = *os[lm->osIndex - 1];
            auto up = o.processSamplesUp (block);
            lm->module->process (up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples());
            o.processSamplesDown (block);
        }
        else
            lm->module->process (l, r, n);

        for (int i = 0; i < n; ++i)
        {
            const double o = outS.getNextValue(), m = mixS.getNextValue(), b = bypassS.getNextValue();
            const double dl = dry[0][(size_t) i], dr = dry[1][(size_t) i];
            const double wl = dl * (1.0 - m) + l[i] * o * m, wr = dr * (1.0 - m) + r[i] * o * m;
            l[i] = dl * b + wl * (1.0 - b);
            r[i] = dr * b + wr * (1.0 - b);
        }
        gr.store (lm->module->info().hasGainReduction ? lm->module->getGainReductionDb() : 0.0f);
        meter (l, r, n, 0.0f);
    }

    static constexpr int kMaxLatency = 4096;

private:
    void swapIfPending() noexcept
    {
        if (pending.load (std::memory_order_relaxed) == nullptr || trash.load (std::memory_order_relaxed) != nullptr)
            return;
        if (auto* p = pending.exchange (nullptr, std::memory_order_acq_rel))
        {
            trash.store (current.exchange (p, std::memory_order_acq_rel), std::memory_order_release);
            resetState();
        }
    }

    void resetState() noexcept
    {
        dryDelay.reset();
        for (auto& o : os) if (o) o->reset();
    }

    void meter (const double* l, const double* r, int n, float) noexcept
    {
        double pk = 0;
        for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i])));
        float cur = outPeak.load();
        while ((float) pk > cur && ! outPeak.compare_exchange_weak (cur, (float) pk)) {}
    }

    SlotParamRefs refs;
    double fs = 44100; int maxBlock = 512;
    std::atomic<LoadedModule*> current { nullptr }, pending { nullptr }, trash { nullptr };
    std::atomic<int> installedType { -1 }, installedOs { 0 }, installedLatency { 0 };
    std::atomic<float> outPeak { 0.0f }, gr { 0.0f };
    std::unique_ptr<juce::dsp::Oversampling<double>> os[2];
    std::vector<double> dry[2];
    DelayLine dryDelay;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> bypassS, mixS, inS, outS;
};

//==============================================================================
class RackEngine
{
public:
    RackEngine() { setOrder ({ 0, 1, 2, 3, 4, 5, 6, 7 }); }

    RackSlot& slot (int i) { return slots[(size_t) i]; }
    const RackSlot& slot (int i) const { return slots[(size_t) i]; }

    void prepare (double fs, int maxBlock) { for (auto& s : slots) s.prepare (fs, maxBlock); }

    /** order[pos] = slot index processed at that chain position. */
    void setOrder (const std::array<int, kNumSlots>& order)
    {
        uint32_t packed = 0;
        for (int p = 0; p < kNumSlots; ++p) packed |= (uint32_t) (order[(size_t) p] & 0xF) << (p * 4);
        packedOrder.store (packed);
    }
    std::array<int, kNumSlots> getOrder() const
    {
        std::array<int, kNumSlots> o {};
        const uint32_t packed = packedOrder.load();
        for (int p = 0; p < kNumSlots; ++p) o[(size_t) p] = (int) ((packed >> (p * 4)) & 0xF);
        return o;
    }

    int totalLatency() const
    {
        int sum = 0;
        for (auto& s : slots) sum += s.getInstalledLatency();
        return sum;
    }

    void process (double* l, double* r, int n, bool soloActive) noexcept
    {
        const uint32_t packed = packedOrder.load (std::memory_order_acquire);
        for (int p = 0; p < kNumSlots; ++p)
            slots[(size_t) ((packed >> (p * 4)) & 0xF)].process (l, r, n, soloActive);
    }

private:
    std::array<RackSlot, kNumSlots> slots;
    std::atomic<uint32_t> packedOrder { 0 };
};
} // namespace bl
