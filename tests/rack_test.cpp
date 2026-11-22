// Bub Leon Mix Rack — end-to-end tests (run with ctest)
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Presets/FactoryPresets.h"
#include <cstdio>
#include <random>

using namespace bl;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("FAIL %s:%d: ", __FILE__, __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static void setReal (MixRackProcessor& p, int slot, const char* suffix, float v)
{
    auto* prm = p.slotParam (slot, suffix);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void setModuleParam (MixRackProcessor& p, int slot, const char* name, double real)
{
    const auto* mi = ModuleRegistry::info (p.getSlotType (slot));
    for (size_t k = 0; k < mi->params.size(); ++k)
        if (mi->params[k].name == name)
            p.slotParam (slot, "p" + juce::String ((int) k + 1))->setValueNotifyingHost ((float) mi->params[k].toNorm (real));
}

template <typename T>
static void fillNoise (juce::AudioBuffer<T>& b, std::mt19937& rng, float amp)
{
    std::uniform_real_distribution<float> d (-amp, amp);
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (c, i, (T) d (rng));
}

static void prepare (MixRackProcessor& p, double fs, int block)
{
    p.setRateAndBufferSizeDetails (fs, block);
    p.prepareToPlay (fs, block);
    p.syncFromMessageThread();
}

//==============================================================================
static void testNullWhenBypassed()
{
    MixRackProcessor p;
    const char* mods[] = { "preamp", "eq4", "fet", "tape", "tubeeq", "limiter", "imager", "opto" };
    for (int s = 0; s < kNumSlots; ++s) p.setSlotType (s, ModuleRegistry::indexOf (mods[s]), true);
    setReal (p, 0, "os", 1);   // preamp 2x
    setReal (p, 3, "os", 2);   // tape 4x
    for (int s = 0; s < kNumSlots; ++s) setReal (p, s, "bypass", 1.0f);
    prepare (p, 48000.0, 256);
    const int lat = p.getLatencySamples();
    CHECK (lat > 0, "expected latency from oversampling/limiter/tape, got %d", lat);

    std::mt19937 rng (1);
    std::vector<float> in, out;
    juce::MidiBuffer midi;
    for (int b = 0; b < 40; ++b)
    {
        juce::AudioBuffer<float> buf (2, 256);
        fillNoise (buf, rng, 0.5f);
        for (int i = 0; i < 256; ++i) in.push_back (buf.getSample (0, i));
        p.processBlock (buf, midi);
        for (int i = 0; i < 256; ++i) out.push_back (buf.getSample (0, i));
    }
    double maxErr = 0;
    for (size_t i = (size_t) lat; i < out.size(); ++i) maxErr = std::max (maxErr, (double) std::abs (out[i] - in[i - (size_t) lat]));
    CHECK (maxErr == 0.0, "bypassed rack must null against the delayed input (max err %g, latency %d)", maxErr, lat);
    std::printf ("null test: latency %d samples, max error %g\n", lat, maxErr);
}

static void testLatencyAlignment()
{
    MixRackProcessor p;
    for (int s = 0; s < kNumSlots; ++s) p.setSlotType (s, -1, true);
    p.setSlotType (0, ModuleRegistry::indexOf ("limiter"), true);
    p.setSlotType (1, ModuleRegistry::indexOf ("eq4"), true);
    prepare (p, 44100.0, 128);
    const int lat = p.getLatencySamples();
    juce::AudioBuffer<float> buf (2, 1024); buf.clear();
    buf.setSample (0, 10, 0.25f); buf.setSample (1, 10, 0.25f);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> chunk (2, 128);
    std::vector<float> out;
    for (int off = 0; off < 1024; off += 128)
    {
        for (int c = 0; c < 2; ++c) chunk.copyFrom (c, 0, buf, c, off, 128);
        p.processBlock (chunk, midi);
        for (int i = 0; i < 128; ++i) out.push_back (chunk.getSample (0, i));
    }
    int peakAt = 0; for (int i = 0; i < 1024; ++i) if (std::abs (out[(size_t) i]) > std::abs (out[(size_t) peakAt])) peakAt = i;
    CHECK (peakAt == 10 + lat, "impulse should appear at %d, found at %d", 10 + lat, peakAt);
    CHECK (std::abs (out[(size_t) peakAt] - 0.25f) < 1e-4f, "impulse level changed: %f", out[(size_t) peakAt]);

    // limiter must hold the ceiling (true peak on, -1 dB)
    setModuleParam (p, 0, "Input", 18.0);
    setModuleParam (p, 0, "Ceiling", -1.0);
    std::mt19937 rng (5);
    float maxOut = 0;
    for (int b = 0; b < 200; ++b)
    {
        juce::AudioBuffer<float> nb (2, 128);
        fillNoise (nb, rng, 0.7f);
        p.processBlock (nb, midi);
        if (b > 4) maxOut = std::max (maxOut, nb.getMagnitude (0, 128));
    }
    CHECK (maxOut <= juce::Decibels::decibelsToGain (-1.0f) + 1e-4f, "limiter exceeded ceiling: %f", maxOut);
    std::printf ("latency test: %d samples, limiter peak %.4f\n", lat, maxOut);
}

static void testAllModulesStable()
{
    std::mt19937 rng (42);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    for (double fs : { 44100.0, 96000.0, 192000.0 })
        for (int type = 0; type < ModuleRegistry::count(); ++type)
            for (int os = 0; os < 3; ++os)
            {
                MixRackProcessor p;
                for (int s = 0; s < kNumSlots; ++s) p.setSlotType (s, -1, true);
                p.setSlotType (0, type, true);
                setReal (p, 0, "os", (float) os);
                prepare (p, fs, 512);
                juce::MidiBuffer midi;
                double worst = 0; bool finite = true;
                for (int b = 0; b < 60; ++b)
                {
                    if (b % 10 == 0)
                        for (int k = 0; k < kParamsPerSlot; ++k)
                            p.slotParam (0, "p" + juce::String (k + 1))->setValueNotifyingHost (u (rng));
                    juce::AudioBuffer<float> buf (2, 512);
                    fillNoise (buf, rng, b % 20 < 10 ? 0.9f : 0.01f);
                    p.processBlock (buf, midi);
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < 512; ++i)
                        {
                            const float v = buf.getSample (c, i);
                            if (! std::isfinite (v)) finite = false;
                            worst = std::max (worst, (double) std::abs (v));
                        }
                }
                CHECK (finite, "%s @ %.0f Hz os %d produced non-finite output", ModuleRegistry::info (type)->id.c_str(), fs, os);
                CHECK (worst < 64.0, "%s @ %.0f Hz os %d output too hot: %f", ModuleRegistry::info (type)->id.c_str(), fs, os, worst);
            }
    std::printf ("module stress test done (%d modules x 3 rates x 3 os)\n", ModuleRegistry::count());
}

static void testStateRecall()
{
    MixRackProcessor a;
    a.setSlotType (0, ModuleRegistry::indexOf ("tape"), true);
    a.setSlotType (3, ModuleRegistry::indexOf ("deess"), true);
    a.setSlotType (5, ModuleRegistry::indexOf ("transient"), true);
    setModuleParam (a, 0, "Drive", 7.5);
    setModuleParam (a, 3, "Frequency", 7200.0);
    setReal (a, 5, "mix", 42.0f);
    a.moveSlotToPosition (5, 0);
    juce::MemoryBlock mb;
    a.getStateInformation (mb);

    MixRackProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    for (int s = 0; s < kNumSlots; ++s)
        CHECK (a.getSlotType (s) == b.getSlotType (s), "slot %d type mismatch %d vs %d", s, a.getSlotType (s), b.getSlotType (s));
    CHECK (a.getOrder() == b.getOrder(), "chain order not restored");
    CHECK (b.getOrder()[0] == 5, "moved slot should be first, got %d", b.getOrder()[0]);
    for (auto* prm : a.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm);
        auto* other = b.apvts.getParameter (rp->getParameterID());
        CHECK (std::abs (rp->getValue() - other->getValue()) < 1e-6f, "param %s not restored", rp->getParameterID().toRawUTF8());
    }
    CHECK (b.slotParam (0, "p1")->getCurrentValueAsText().contains ("7.5"), "dynamic value text wrong: %s",
           b.slotParam (0, "p1")->getCurrentValueAsText().toRawUTF8());
    CHECK (b.slotParam (0, "p1")->getName (64).contains ("Drive"), "dynamic name wrong: %s", b.slotParam (0, "p1")->getName (64).toRawUTF8());
    std::printf ("state recall test done\n");
}

static void testFactoryPresets()
{
    MixRackProcessor p;
    prepare (p, 48000.0, 512);
    std::mt19937 rng (7);
    juce::MidiBuffer midi;
    const auto& presets = factoryPresets();
    CHECK (presets.size() >= 30, "expected 30 factory presets, have %d", (int) presets.size());
    for (auto& def : presets)
    {
        p.applyRackPreset (def);
        p.syncFromMessageThread();
        for (int s = 0; s < (int) def.slots.size(); ++s)
            if (def.slots[(size_t) s].module)
            {
                CHECK (p.getSlotType (s) == ModuleRegistry::indexOf (def.slots[(size_t) s].module), "%s: slot %d wrong module", def.name, s);
                const auto* mi = ModuleRegistry::info (p.getSlotType (s));
                for (auto& [name, v] : def.slots[(size_t) s].values)
                {
                    bool found = false;
                    for (auto& ps : mi->params) found |= ps.name == name;
                    CHECK (found, "%s: unknown parameter '%s' for %s", def.name, name, mi->id.c_str());
                }
            }
        double peak = 0, inPeak = 0; bool finite = true;
        for (int b = 0; b < 100; ++b)
        {
            juce::AudioBuffer<float> buf (2, 512);
            fillNoise (buf, rng, 0.2f);   // ~ -19 dBFS RMS
            inPeak = std::max (inPeak, (double) buf.getMagnitude (0, 512));
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) finite &= std::isfinite (buf.getSample (c, i));
            if (b > 20) peak = std::max (peak, (double) buf.getMagnitude (0, 512));
        }
        CHECK (finite, "%s: non-finite output", def.name);
        CHECK (peak < 1.5, "%s: output too hot (%.2f dBFS)", def.name, juce::Decibels::gainToDecibels (peak));
        CHECK (peak > 0.02, "%s: output too quiet (%.2f dBFS)", def.name, juce::Decibels::gainToDecibels (peak));
    }
    std::printf ("factory presets test done (%d presets)\n", (int) presets.size());
}

static void testLayoutsAndDouble()
{
    for (auto [inSet, outSet] : { std::pair { juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono() },
                                  std::pair { juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono() },
                                  std::pair { juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo() } })
    {
        MixRackProcessor p;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (inSet); layout.outputBuses.add (outSet);
        CHECK (p.setBusesLayout (layout), "layout %d->%d rejected", inSet.size(), outSet.size());
        prepare (p, 48000.0, 256);
        juce::AudioBuffer<float> buf (juce::jmax (inSet.size(), outSet.size()), 256);
        std::mt19937 rng (3);
        juce::MidiBuffer midi;
        for (int b = 0; b < 20; ++b) { fillNoise (buf, rng, 0.3f); p.processBlock (buf, midi); }
        bool finite = true;
        for (int c = 0; c < buf.getNumChannels(); ++c) for (int i = 0; i < 256; ++i) finite &= std::isfinite (buf.getSample (c, i));
        CHECK (finite, "non-finite output for %d->%d", inSet.size(), outSet.size());
    }
    {
        MixRackProcessor p;
        p.setProcessingPrecision (juce::AudioProcessor::doublePrecision);
        prepare (p, 96000.0, 333);
        juce::AudioBuffer<double> buf (2, 1000);   // larger than the prepared block size
        std::mt19937 rng (9);
        juce::MidiBuffer midi;
        for (int b = 0; b < 10; ++b) { fillNoise (buf, rng, 0.5f); p.processBlock (buf, midi); }
        bool finite = true;
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 1000; ++i) finite &= std::isfinite (buf.getSample (c, i));
        CHECK (finite, "double path produced non-finite output");
    }
    std::printf ("layouts + double precision test done\n");
}

static void testUndoAndModuleSwapLive()
{
    MixRackProcessor p;
    prepare (p, 48000.0, 256);
    juce::MidiBuffer midi;
    std::mt19937 rng (11);
    const int before = p.getSlotType (2);
    p.undoManager.beginNewTransaction();
    p.setSlotType (2, ModuleRegistry::indexOf ("fet"), true);
    juce::AudioBuffer<float> buf (2, 256);
    for (int i = 0; i < 30; ++i)
    {
        if (i % 5 == 0) p.setSlotType (3, i % ModuleRegistry::count(), true);   // swap while "playing"
        fillNoise (buf, rng, 0.4f);
        p.processBlock (buf, midi);
        p.syncFromMessageThread();
    }
    p.undoManager.beginNewTransaction();
    p.undoManager.undo(); p.undoManager.undo(); p.undoManager.undo(); p.undoManager.undo(); p.undoManager.undo(); p.undoManager.undo(); p.undoManager.undo();
    CHECK (p.getSlotType (2) == before, "undo should restore slot 2 module (%d vs %d)", p.getSlotType (2), before);
    std::printf ("undo + live swap test done\n");
}

static void testVintage76()
{
    // level-neutral at low drive, more harmonics as the gain rises, hum switch works
    auto run = [] (double gainDb, float amp, bool hum, double& levelDb, double& thd)
    {
        MixRackProcessor p;
        for (int s = 0; s < kNumSlots; ++s) p.setSlotType (s, -1, true);
        p.setSlotType (0, ModuleRegistry::indexOf ("v76"), true);
        setModuleParam (p, 0, "Gain", gainDb);
        setModuleParam (p, 0, "Hum", hum ? 1.0 : 0.0);
        setModuleParam (p, 0, "Tube Age", 0);
        prepare (p, 48000.0, 480);
        juce::MidiBuffer midi;
        std::vector<double> out;
        const double f = 1000.0;
        long long t = 0;
        for (int b = 0; b < 200; ++b)
        {
            juce::AudioBuffer<float> buf (2, 480);
            for (int i = 0; i < 480; ++i, ++t)
            {
                const float v = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * f * (double) t / 48000.0);
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            p.processBlock (buf, midi);
            if (b >= 100) for (int i = 0; i < 480; ++i) out.push_back (buf.getSample (0, i));
        }
        auto bin = [&] (double freq)
        {
            double re = 0, im = 0;
            for (size_t i = 0; i < out.size(); ++i)
            {
                const double ph = 2.0 * juce::MathConstants<double>::pi * freq * (double) i / 48000.0;
                re += out[i] * std::cos (ph); im += out[i] * std::sin (ph);
            }
            return 2.0 * std::sqrt (re * re + im * im) / (double) out.size();
        };
        const double fund = bin (f);
        double harm = 0; for (int k = 2; k <= 6; ++k) harm += std::pow (bin (f * k), 2.0);
        levelDb = juce::Decibels::gainToDecibels (fund / amp);
        thd = std::sqrt (harm) / fund;
        if (hum) thd = bin (50.0);   // report hum amplitude instead
    };
    double lvl, thdLow, thdHigh, humAmp;
    run (0.0, 0.03f, false, lvl, thdLow);
    CHECK (std::abs (lvl) < 1.0, "V76 should be level-neutral at low drive (%.2f dB)", lvl);
    run (0.0, 0.25f, false, lvl, thdLow);
    run (30.0, 0.25f, false, lvl, thdHigh);
    CHECK (thdHigh > thdLow * 3.0 && thdHigh > 0.01, "V76 harmonics should rise with gain (%.4f -> %.4f)", thdLow, thdHigh);
    run (0.0, 0.0f + 1e-6f, true, lvl, humAmp);
    CHECK (humAmp > 1e-5 && humAmp < 1e-3, "V76 hum level unexpected: %g", humAmp);
    std::printf ("V76 test: THD %.3f %% -> %.3f %%, hum %.1f dBFS\n", thdLow * 100, thdHigh * 100, juce::Decibels::gainToDecibels (humAmp));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testNullWhenBypassed();
    testLatencyAlignment();
    testStateRecall();
    testFactoryPresets();
    testLayoutsAndDouble();
    testUndoAndModuleSwapLive();
    testVintage76();
    testAllModulesStable();
    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
