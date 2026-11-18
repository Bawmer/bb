#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace ui
{
    const juce::Colour bg       { 0xff0b1020 };
    const juce::Colour panel    { 0xff141b30 };
    const juce::Colour panelHi  { 0xff1c2542 };
    const juce::Colour gold     { 0xffe8b04a };
    const juce::Colour goldDim  { 0xff8a6a2e };
    const juce::Colour ice      { 0xff6fd3ff };
    const juce::Colour text     { 0xffdfe4f2 };
    const juce::Colour textDim  { 0xff7d87a6 };
    const juce::Colour voiceCol[4] = { juce::Colour (0xffe8b04a), juce::Colour (0xff6fd3ff),
                                       juce::Colour (0xffff7aa8), juce::Colour (0xff8cf09a) };
}

class HarmonyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HarmonyLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hl, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (14.0f, juce::Font::bold)); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hl, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool hl, bool down) override;
};

// Keyboard strip showing sung pitch and the harmony targets
class PitchKeyboard : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    float inputMidi = -1.0f;
    float voiceMidi[4] { -1, -1, -1, -1 };
    static constexpr int lowNote = 36, highNote = 84; // C2..C6
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label,
          juce::Colour accent = ui::gold, bool small = false);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
    juce::String caption;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    juce::Colour accent;
};

class VoiceStrip : public juce::Component
{
public:
    VoiceStrip (juce::AudioProcessorValueTreeState&, int index);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh (int mode, int scale, float targetMidi, int heldNotes);
private:
    int index;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    Knob interval, level, pan, detune;
    juce::String intervalName, noteName;
    juce::AudioProcessorValueTreeState& state;
};

// A rendered file you can drag straight onto a DAW track
class DragChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    DragChip (juce::String label, juce::StringArray paths, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
private:
    juce::String label;
    juce::StringArray paths;
    juce::Colour accent;
    bool hover = false, dragging = false;
};

// Waveform of the loaded / captured stem; click to load, or drop a file on the plugin
class StemZone : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
    std::function<void()> onClick;
    std::vector<float> overview;
    juce::String name, info;
    bool dragHover = false;
};

class HarmonyStackEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit HarmonyStackEditor (HarmonyStackProcessor&);
    ~HarmonyStackEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { stemZone.dragHover = true; stemZone.repaint(); }
    void fileDragExit (const juce::StringArray&) override              { stemZone.dragHover = false; stemZone.repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void loadStemFile (const juce::File&);
    void rebuildChips();
    void refreshStemZone();
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    HarmonyStackProcessor& proc;
    HarmonyLookAndFeel lnf;

    juce::ComboBox modeBox, keyBox, scaleBox, presetBox;
    std::unique_ptr<ComboAtt> modeAtt, keyAtt, scaleAtt;
    PitchKeyboard keyboard;
    std::unique_ptr<VoiceStrip> strips[4];

    juce::ToggleButton doubleBtn { "DOUBLE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> doubleAtt;
    Knob humanize, width, formant, breath, glide, vib, vibRate, gate, dry, harmony, output;

    // Auto Harmony panel
    StemZone stemZone;
    juce::TextButton captureBtn { "CAPTURE" }, generateBtn { "GENERATE" }, previewBtn { "PREVIEW" }, folderBtn { "FOLDER" };
    juce::ToggleButton autoKeyBtn { "AUTO KEY" }, allStylesBtn { "ALL STYLES" }, stemsBtn { "STEMS" };
    juce::Viewport chipView;
    juce::Component chipHolder;
    juce::OwnedArray<DragChip> chips;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 500 };
    juce::String statusText { "Drop a vocal stem or press CAPTURE and play your track" };
    int seenResult = -1, seenStem = -1;

    juce::String pitchText { "--" };
    float meter = 0.0f;
    bool voiced = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonyStackEditor)
};
