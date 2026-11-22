#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "GUI/RackLookAndFeel.h"
#include "GUI/Widgets.h"

class MixRackProcessor;

namespace bl
{
/** One vertical rack unit showing the module loaded in a slot. */
class ModuleUnit final : public juce::Component,
                         public juce::DragAndDropTarget
{
public:
    static constexpr int kWidth = 148, kHeight = 548;

    ModuleUnit (MixRackProcessor&, int slotIndex);
    ~ModuleUnit() override;

    int getSlot() const noexcept { return slot; }
    std::function<int()> getPosition;   // chain position (set by the editor)

    /** Rebuild controls if the slot's module changed. */
    void refresh();
    void updateMeters();

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
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
        std::unique_ptr<juce::Slider> knob;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::TextButton> toggle;
        std::unique_ptr<juce::SliderParameterAttachment> knobAtt;
        std::unique_ptr<juce::ParameterAttachment> paramAtt;
    };

    void rebuild();
    void showMenu();
    std::unique_ptr<Cell> makeKnobCell (juce::RangedAudioParameter&, const juce::String& label, double defNorm, bool bipolar, int size);
    void styleKnob (juce::Slider&, bool bipolar);
    juce::Rectangle<int> gripArea() const { return { 50, 34, 46, 18 }; }

    MixRackProcessor& proc;
    const int slot;
    int shownType = -999;
    bool dragOver = false;

    juce::ComboBox moduleBox, osBox;
    juce::TextButton menuButton { juce::String::fromUTF8 ("\xe2\x89\xa1") }, soloButton { "S" };
    LedButton power { juce::Colour (0xffff5a3a), true };
    std::unique_ptr<juce::ButtonParameterAttachment> powerAtt, soloAtt;
    std::unique_ptr<juce::ComboBoxParameterAttachment> osAtt;

    std::vector<std::unique_ptr<Cell>> cells, trims;
    LevelBar outBar { false }, grBar { false, true };
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleUnit)
};
} // namespace bl
