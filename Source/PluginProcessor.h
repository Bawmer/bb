#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/HarmonyEngine.h"
#include "Presets.h"
#include "Labels.h"
#include "HarmonyRenderer.h"
#include <atomic>

namespace ID
{
    inline const juce::String mode = "mode", key = "key", scale = "scale",
        humanize = "humanize", dbl = "double", dblWidth = "dblWidth", formant = "formant",
        breath = "breath", glide = "glide", vibDepth = "vibDepth", vibRate = "vibRate", gate = "gate",
        dry = "dry", harmony = "harmony", output = "output";
    inline juce::String vOn (int v)     { return "v" + juce::String (v + 1) + "On"; }
    inline juce::String vInt (int v)    { return "v" + juce::String (v + 1) + "Interval"; }
    inline juce::String vLevel (int v)  { return "v" + juce::String (v + 1) + "Level"; }
    inline juce::String vPan (int v)    { return "v" + juce::String (v + 1) + "Pan"; }
    inline juce::String vDetune (int v) { return "v" + juce::String (v + 1) + "Detune"; }
}

class HarmonyStackProcessor : public juce::AudioProcessor
{
public:
    HarmonyStackProcessor();
    ~HarmonyStackProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- Auto Harmony: offline render -> drag & drop (message thread API) ----
    bool loadStem (const juce::File& file, juce::String& error);
    void armCapture (bool shouldArm);
    bool isCaptureArmed() const noexcept  { return captureArmed.load(); }
    bool isCapturing() const noexcept     { return captureActive.load(); }
    double capturedSeconds() const noexcept { return captureLen.load() / juce::jmax (1.0, getSampleRate()); }
    bool startGenerate (juce::String& error);
    void cancelGenerate()                 { renderer.cancel(); }
    void pollBackground();                // call regularly from the message thread
    void setPreviewPlaying (bool shouldPlay);
    bool isPreviewPlaying() const noexcept { return previewPlaying.load(); }
    float previewProgress() const noexcept { return previewLen > 0 ? (float) previewPos.load() / (float) previewLen : 0.0f; }
    hs::EngineSnapshot makeSnapshot();

    std::shared_ptr<juce::AudioBuffer<float>> stem;   // message thread only
    double stemSampleRate = 44100.0;
    juce::String stemName;
    std::vector<float> stemOverview;                  // peak overview for the waveform display
    HarmonyRenderer renderer;
    HarmonyRenderer::Result lastResult;
    int resultSerial = 0, stemSerial = 0;
    bool autoKey = true, allStyles = false, voiceStems = true;

    // GUI feedback
    std::atomic<float> uiInputMidi { -1.0f };
    std::array<std::atomic<float>, hs::HarmonyEngine::kVoices> uiVoiceMidi;
    std::atomic<float> uiOutLevel { 0.0f };
    std::atomic<int>   uiHeldNotes { 0 };

private:
    void pullParameters();
    void setStem (std::shared_ptr<juce::AudioBuffer<float>> b, double sr, const juce::String& name);

    juce::AudioFormatManager formats;
    juce::AudioBuffer<float> captureBuf;
    std::atomic<int> captureLen { 0 };
    std::atomic<bool> captureArmed { false }, captureActive { false }, captureDone { false };
    juce::SpinLock previewLock;
    juce::AudioBuffer<float> previewBuf;
    std::atomic<int> previewPos { 0 };
    int previewLen = 0;
    std::atomic<bool> previewPlaying { false };

    hs::HarmonyEngine engine;
    juce::AudioBuffer<float> monoBuf, wetBuf, dryDelay;
    int dryWrite = 0, dryLen = 1, chunk = 512;
    juce::SmoothedValue<float> dryGain, harmGain, outGain;
    int currentProgram = 0;
    int heldCount = 0;

    struct Raw
    {
        std::atomic<float> *mode{}, *key{}, *scale{}, *humanize{}, *dbl{}, *dblWidth{}, *formant{},
                           *breath{}, *glide{}, *vibDepth{}, *vibRate{}, *gate{}, *dry{}, *harmony{}, *output{};
        std::atomic<float> *on[4]{}, *interval[4]{}, *level[4]{}, *pan[4]{}, *detune[4]{};
    } raw;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonyStackProcessor)
};
