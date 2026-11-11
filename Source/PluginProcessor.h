#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "OxfordUltraLimiter.h"
#include "LoudnessMeterITU.h"
#include "OxfordHistoryScope.h"

namespace BubLeonDSP
{
    namespace ParamIDs
    {
        inline constexpr auto inputGain       = "inputGain";
        inline constexpr auto threshold       = "threshold";
        inline constexpr auto attack          = "attack";
        inline constexpr auto release         = "release";
        inline constexpr auto autoRelease     = "autoRelease";
        inline constexpr auto enhance         = "enhance";
        inline constexpr auto transientPunch  = "transientPunch";
        inline constexpr auto crossoverHz     = "crossoverHz";
        inline constexpr auto lfHeadroom      = "lfHeadroom";
        inline constexpr auto bandCoupling    = "bandCoupling";
        inline constexpr auto msMode          = "msMode";
        inline constexpr auto midSideLink     = "midSideLink";
        inline constexpr auto sideLowMono     = "sideLowMono";
        inline constexpr auto enhanceSideBoost= "enhanceSideBoost";
        inline constexpr auto stereoWidth     = "stereoWidth";
        inline constexpr auto ditherBits      = "ditherBits";
        inline constexpr auto ditherShaper    = "ditherShaper";
        inline constexpr auto autoBlanking    = "autoBlanking";
        inline constexpr auto autoGainMatch   = "autoGainMatch";
        inline constexpr auto oversampling    = "oversampling";
    }
}

class OxfordUltraAudioProcessor final : public juce::AudioProcessor
{
public:
    OxfordUltraAudioProcessor();
    ~OxfordUltraAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    // Factory presets exposed as host programs (see Presets.h)
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    BubLeonDSP::ScopeDataCollector     scopeCollector;

    BubLeonDSP::LoudnessTelemetry getLatestLoudnessTelemetry() const noexcept
    {
        return loudnessAnalyzer.getTelemetry();
    }

    // Message thread: clears the integrated-loudness history on the next audio block.
    void requestLoudnessReset() noexcept { loudnessResetRequested.store(true, std::memory_order_release); }

private:
    void syncParametersFromAPVTS() noexcept;
    void updateLatencyForEngine() noexcept;
    void processChunk(juce::AudioBuffer<float>& chunk) noexcept;

    // 4x Linear-Phase FIR Oversampler for True-Peak / ADAA Precision
    juce::dsp::Oversampling<float> oversampler4x {
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true  // integer latency for exact PDC
    };

    BubLeonDSP::OxfordUltraLimiter  limiter4x;
    BubLeonDSP::OxfordUltraLimiter  limiter1x; // Low-latency live tracking engine
    BubLeonDSP::LoudnessAnalyzerITU loudnessAnalyzer;
    juce::AudioBuffer<float>        dryInputCopy;

    bool lastOsActive = true;
    double currentSampleRate = 48000.0;
    int maxBlockSize = 512;                       // oversampler + scratch capacity; larger host blocks are split
    std::atomic<bool> loudnessResetRequested { false };
    std::atomic<int>  currentProgram { 0 };      // thread-safe mirror of state property "presetIndex"

    // Lock-free cached raw atomic pointers for O(1) audio thread reads
    struct AtomicParamPointers
    {
        std::atomic<float>* inputGain        = nullptr;
        std::atomic<float>* threshold        = nullptr;
        std::atomic<float>* attack           = nullptr;
        std::atomic<float>* release          = nullptr;
        std::atomic<float>* autoRelease      = nullptr;
        std::atomic<float>* enhance          = nullptr;
        std::atomic<float>* transientPunch   = nullptr;
        std::atomic<float>* crossoverHz      = nullptr;
        std::atomic<float>* lfHeadroom       = nullptr;
        std::atomic<float>* bandCoupling     = nullptr;
        std::atomic<float>* msMode           = nullptr;
        std::atomic<float>* midSideLink      = nullptr;
        std::atomic<float>* sideLowMono      = nullptr;
        std::atomic<float>* enhanceSideBoost = nullptr;
        std::atomic<float>* stereoWidth      = nullptr;
        std::atomic<float>* ditherBits       = nullptr;
        std::atomic<float>* ditherShaper     = nullptr;
        std::atomic<float>* autoBlanking     = nullptr;
        std::atomic<float>* autoGainMatch    = nullptr;
        std::atomic<float>* oversampling     = nullptr;
    } paramPtrs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OxfordUltraAudioProcessor)
};
