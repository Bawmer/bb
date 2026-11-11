#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "OxfordHistoryScope.h"
#include "FaceplateLookAndFeel.h"

// Photoreal faceplate editor. All layout coordinates are faceplate pixels (1295 x 832),
// scaled uniformly; the window is freely resizable at a fixed aspect ratio.
class OxfordUltraAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit OxfordUltraAudioProcessorEditor(OxfordUltraAudioProcessor&);
    ~OxfordUltraAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int kFaceW = 1295;
    static constexpr int kFaceH = 832;

private:
    void timerCallback() override;
    void showSettingsMenu();
    void showDitherMenu();
    void setChoiceParam(const char* paramID, int index);
    juce::Rectangle<int> face(float x, float y, float w, float h) const;
    juce::Rectangle<int> knobBounds(float cx, float cy) const;

    OxfordUltraAudioProcessor& processorRef;
    BubLeonGui::FaceplateLookAndFeel lnf;
    juce::Image faceplate;

    BubLeonDSP::OxfordHistoryScope historyScope;
    BubLeonDSP::OxfordLoudnessHud  hud;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct MainKnob
    {
        BubLeonGui::FaceKnob slider;
        BubLeonGui::ValueBox box;
        std::unique_ptr<SliderAttachment> attachment;
    };
    std::array<MainKnob, 14> knobs;

    struct MacroKnob
    {
        BubLeonGui::FaceKnob slider;
        std::unique_ptr<SliderAttachment> attachment;
    };
    std::array<MacroKnob, 3> macros;               // top-right column: input gain, ceiling, enhance

    BubLeonGui::FaceKnob scopeRangeKnob, scopeTimeKnob;   // the two big caps beside the screen

    BubLeonGui::LedButton msToggle, matchToggle, osToggle;
    std::unique_ptr<ButtonAttachment> msAttach, matchAttach, osAttach;

    struct LcdDisplay final : public juce::Component, public juce::SettableTooltipClient
    {
        juce::String line1 { "BL Bio-Sync" }, line2 { "All Systems Nominal" }, line3 { "Settings..." };
        std::function<void()> onClick;
        void paint(juce::Graphics&) override;
        void mouseUp(const juce::MouseEvent& e) override { if (onClick && e.mouseWasClicked()) onClick(); }
    } lcd;

    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OxfordUltraAudioProcessorEditor)
};
