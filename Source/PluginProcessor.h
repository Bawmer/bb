#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine/RackEngine.h"
#include "DSP/Meters.h"

namespace bl { struct RackPresetDef; }

class MixRackProcessor final : public juce::AudioProcessor,
                               private juce::ValueTree::Listener,
                               private juce::Timer
{
public:
    MixRackProcessor();
    ~MixRackProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    // Rack control (message thread)
    int  getSlotType (int slot) const noexcept { return slotType[(size_t) slot].load(); }
    void setSlotType (int slot, int type, bool resetParams);
    std::array<int, bl::kNumSlots> getOrder() const { return engine.getOrder(); }
    void moveSlotToPosition (int slot, int newPos);
    void resetSlotParams (int slot);
    juce::RangedAudioParameter* slotParam (int slot, const juce::String& suffix) const;

    /** Module presets (.blmod) */
    juce::ValueTree makeModuleState (int slot) const;
    void loadModuleState (int slot, const juce::ValueTree& state);
    void copyModule (int slot)  { clipboard = makeModuleState (slot); }
    void pasteModule (int slot) { if (clipboard.isValid()) loadModuleState (slot, clipboard); }
    bool hasClipboard() const   { return clipboard.isValid(); }

    /** Rack presets */
    void applyRackPreset (const bl::RackPresetDef&);
    void loadInitRack();
    bool saveUserPreset (const juce::File&);
    bool loadUserPreset (const juce::File&);
    juce::String getPresetName() const              { const juce::SpinLock::ScopedLockType l (nameLock); return presetName; }
    void setPresetName (const juce::String& n)      { const juce::SpinLock::ScopedLockType l (nameLock); presetName = n; }

    /** A/B compare */
    void selectAB (int which);
    void copyCurrentToOther();
    int  getActiveAB() const noexcept { return activeAB; }

    /** Call regularly from the message thread (the timer does it; tests call it directly). */
    void syncFromMessageThread();

private:
    std::array<std::atomic<int>, bl::kNumSlots> slotType {};   // declared first: parameters reference it

public:
    //==========================================================================
    juce::UndoManager undoManager { 30000, 30 };
    juce::AudioProcessorValueTreeState apvts;
    bl::RackEngine engine;
    bl::LevelMeter inMeter, outMeter;
    std::atomic<float> cpuLoad { 0.0f };

    static juce::String slotId (int slot, const juce::String& suffix) { return "s" + juce::String (slot + 1) + "_" + suffix; }
    static juce::File userPresetFolder();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (MixRackProcessor&);
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeRedirected (juce::ValueTree&) override;
    void timerCallback() override { syncFromMessageThread(); }

    void applyTypeFromTree (int slot);
    void applyOrderFromTree();
    void applyAllFromTree();
    int  desiredOsIndex (int slot) const;
    void rebuildSlot (int slot, int type);

    template <typename T> void processT (juce::AudioBuffer<T>&);
    template <typename T> void bypassT (juce::AudioBuffer<T>&);
    std::vector<double> bypassBuf[2];
    int bypassPos = 0;

    std::atomic<float>* soloRefs[bl::kNumSlots] {};
    std::atomic<float>* osRefs[bl::kNumSlots] {};
    std::atomic<float>* inGainRef = nullptr;
    std::atomic<float>* outGainRef = nullptr;
    std::atomic<float>* osGlobalRef = nullptr;

    std::vector<double> work[2];
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> inGainS, outGainS;
    double sr = 44100.0; int maxBlock = 512; bool prepared = false;
    std::recursive_mutex installMutex;

    juce::ValueTree abState[2], clipboard;
    juce::String presetName { "Init" };
    mutable juce::SpinLock nameLock;     // the host may restore state off the message thread
    int activeAB = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixRackProcessor)
};
