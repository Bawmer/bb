#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GUI/ModuleUnit.h"
#include "GUI/Skin.h"

/** Amber LCD showing the preset; click = preset menu, wheel / arrows = previous / next, right-click = more. */
class PresetDisplay final : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick, onMenu;
    std::function<void (int)> onStep;
    juce::String name { "Init" };
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
};

/** Editor content at the artwork's native size (2032 x 1120); the editor scales it. */
class RackContent final : public juce::Component,
                          public juce::DragAndDropContainer,
                          private juce::Timer
{
public:
    static constexpr int kWidth = bl::skin::kW, kHeight = bl::skin::kH;

    explicit RackContent (MixRackProcessor&);
    ~RackContent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    juce::PopupMenu buildPresetMenu();
    void loadPresetId (int id);
    void stepPreset (int dir);
    void savePreset();
    void showPresetExtras();
    void showOsMenu();
    void refreshAll();
    void refreshAB();
    juce::Rectangle<int> unitRect (int pos) const;

    MixRackProcessor& proc;
    juce::Image bg;
    std::array<std::unique_ptr<bl::ModuleUnit>, bl::kNumSlots> units;
    std::array<int, bl::kNumSlots> shownOrder {};

    PresetDisplay presetDisplay;
    juce::TextButton saveButton { "SAVE" }, aButton { "A" }, bButton { "B" }, copyAB { "A=B" };
    juce::Slider inGain, outGain;
    std::unique_ptr<juce::SliderParameterAttachment> inAtt, outAtt;
    bl::VuMeter inVu { false }, outVu { false };
    std::array<std::unique_ptr<bl::GrLeds>, bl::kNumSlots> grLeds;
    juce::String latencyText { "LATENCY -" }, cpuText { "-" }, lufsText { "-" };
    std::vector<juce::File> userPresets;
    int currentPresetId = 1;
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
