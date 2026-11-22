#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GUI/ModuleUnit.h"

/** Unscaled editor content; the editor scales it 100-200 %. */
class RackContent final : public juce::Component,
                          public juce::DragAndDropContainer,
                          private juce::Timer
{
public:
    static constexpr int kTopH = 54, kBottomH = 86, kGap = 6, kMargin = 12;
    static constexpr int kWidth  = kMargin * 2 + bl::kNumSlots * bl::ModuleUnit::kWidth + (bl::kNumSlots - 1) * kGap;
    static constexpr int kHeight = kTopH + bl::ModuleUnit::kHeight + kBottomH + 10;

    explicit RackContent (MixRackProcessor&);
    ~RackContent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void buildPresetMenu();
    void refreshAB();

    MixRackProcessor& proc;
    std::array<std::unique_ptr<bl::ModuleUnit>, bl::kNumSlots> units;
    std::array<int, bl::kNumSlots> shownOrder {};

    juce::ComboBox presetBox, osModeBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" },
                     aButton { "A" }, bButton { "B" }, copyAB { "A>B" },
                     undoButton { "UNDO" }, redoButton { "REDO" };
    juce::Slider inGain, outGain;
    std::unique_ptr<juce::SliderParameterAttachment> inAtt, outAtt;
    std::unique_ptr<juce::ComboBoxParameterAttachment> osAtt;
    bl::LevelBar inL { false }, inR { false }, outL { false }, outR { false };
    std::array<std::unique_ptr<bl::LevelBar>, bl::kNumSlots> grStrip;
    juce::String latencyText { "LATENCY -" }, cpuText { "CPU -" }, lufsText { "-inf" };
    std::vector<juce::File> userPresets;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };
};

class MixRackEditor final : public juce::AudioProcessorEditor
{
public:
    explicit MixRackEditor (MixRackProcessor&);
    ~MixRackEditor() override;
    void resized() override;

private:
    MixRackProcessor& proc;
    bl::RackLookAndFeel lnf;
    RackContent content;
};
