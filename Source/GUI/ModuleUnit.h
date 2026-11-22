#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "GUI/RackLookAndFeel.h"
#include "GUI/Widgets.h"

class MixRackProcessor;

namespace bl
{
/** One rack module (219 x 603, the artwork's unit size) showing the module loaded in a slot. */
class ModuleUnit final : public juce::Component,
                         public juce::DragAndDropTarget
{
public:
    static constexpr int kWidth = 219, kHeight = 603;

    ModuleUnit (MixRackProcessor&, int slotIndex);
    ~ModuleUnit() override;

    int getSlot() const noexcept { return slot; }
    std::function<int()> getPosition;   // chain position (set by the editor)

    void refresh();
    void updateMeters();

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dragOver = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    static juce::Image faceplateImage (const std::string& moduleId);

private:
    struct Cell
    {
        juce::String label;
        juce::Rectangle<int> area;
        juce::RangedAudioParameter* param = nullptr;
        bool hasLed = false;
        std::unique_ptr<juce::Slider> knob;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<ToggleSwitch> toggle;
        std::unique_ptr<juce::SliderParameterAttachment> knobAtt;
        std::unique_ptr<juce::ParameterAttachment> paramAtt;
    };

    void rebuild();
    void showModuleMenu();
    void runMenuAction (int id);
    std::unique_ptr<Cell> makeKnobCell (juce::RangedAudioParameter&, const juce::String& label, double defNorm, bool trim);
    void styleKnob (juce::Slider&);
    juce::Rectangle<int> titleArea() const { return { 46, 10, kWidth - 70, 38 }; }
    bool isEmpty() const { return shownType < 0; }

    MixRackProcessor& proc;
    const int slot;
    int shownType = -999;
    bool dragOver = false, dragStarted = false;
    int repaintTick = 0;
    int gridCols = 2, labelSize = 13;

    LedButton power { juce::Colour (0xffff4a2a), true }, solo { juce::Colour (0xffffb020), false };
    juce::ComboBox osBox;
    std::unique_ptr<juce::ButtonParameterAttachment> powerAtt, soloAtt;
    std::unique_ptr<juce::ComboBoxParameterAttachment> osAtt;

    std::vector<std::unique_ptr<Cell>> cells, trims;
    VuMeter vu { true };
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleUnit)
};
} // namespace bl
