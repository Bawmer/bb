#include "GUI/ModuleUnit.h"
#include "PluginProcessor.h"
#include <map>

namespace bl
{
namespace
{
constexpr int kTop = 58, kCellW = 66, kCellH = 60, kCols = 2, kRows = 6;
const char* kModuleExt = "*.blmod";

juce::File moduleFolder() { return MixRackProcessor::userPresetFolder().getChildFile ("Modules"); }
}

juce::Image ModuleUnit::faceplateImage (const std::string& id)
{
    static std::map<std::string, juce::Image> cache;   // message thread only; loaded once per session
    auto it = cache.find (id);
    if (it != cache.end()) return it->second;
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("BL Audio/Mix Rack/Faceplates").getChildFile (juce::String (id) + ".png");
    auto img = f.existsAsFile() ? juce::ImageFileFormat::loadFrom (f) : juce::Image();
    cache[id] = img;
    return img;
}

ModuleUnit::ModuleUnit (MixRackProcessor& p, int s) : proc (p), slot (s)
{
    setSize (kWidth, kHeight);

    moduleBox.addItem ("- Empty -", 1);
    juce::StringArray cats { "Color", "EQ", "Dynamics", "Utility" };
    for (auto& cat : cats)
    {
        moduleBox.addSectionHeading (cat);
        for (int t = 0; t < ModuleRegistry::count(); ++t)
            if (ModuleRegistry::info (t)->category == cat.toStdString())
                moduleBox.addItem (ModuleRegistry::info (t)->name, t + 2);
    }
    moduleBox.onChange = [this]
    {
        const int type = moduleBox.getSelectedId() - 2;
        if (type != proc.getSlotType (slot))
        {
            proc.undoManager.beginNewTransaction ("Change module");
            proc.setSlotType (slot, type, true);
            refresh();
        }
    };
    moduleBox.setTooltip ("Choose the module for this slot");
    addAndMakeVisible (moduleBox);

    osBox.addItemList ({ "1x", "2x", "4x" }, 1);
    osBox.setTooltip ("Oversampling for this module");
    osAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*proc.slotParam (slot, "os"), osBox, &proc.undoManager);
    addAndMakeVisible (osBox);

    power.setTooltip ("Module on / bypass");
    powerAtt = std::make_unique<juce::ButtonParameterAttachment> (*proc.slotParam (slot, "bypass"), power, &proc.undoManager);
    addAndMakeVisible (power);

    soloButton.setClickingTogglesState (true);
    soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xfff1c232));
    soloButton.setTooltip ("Solo: listen to this module only (all others bypassed)");
    soloAtt = std::make_unique<juce::ButtonParameterAttachment> (*proc.slotParam (slot, "solo"), soloButton, &proc.undoManager);
    addAndMakeVisible (soloButton);

    menuButton.setTooltip ("Module presets, copy / paste, reset");
    menuButton.onClick = [this] { showMenu(); };
    addAndMakeVisible (menuButton);

    addAndMakeVisible (outBar);
    addAndMakeVisible (grBar);

    // trims: input, mix, output
    auto addTrim = [this] (const char* suffix, const char* label, double defNorm, bool bipolar)
    {
        trims.push_back (makeKnobCell (*proc.slotParam (slot, suffix), label, defNorm, bipolar, 42));
    };
    addTrim ("in", "IN", 0.5, true);
    addTrim ("mix", "MIX", 1.0, false);
    addTrim ("out", "OUT", 0.5, true);

    refresh();
}

ModuleUnit::~ModuleUnit() = default;

void ModuleUnit::styleKnob (juce::Slider& k, bool bipolar)
{
    const auto* mi = ModuleRegistry::info (proc.getSlotType (slot));
    k.getProperties().set ("plate", (int) (mi ? mi->faceplate : Faceplate::Black));
    k.getProperties().set ("accent", (juce::int64) (mi ? mi->accent : 0xffe0a040));
    k.getProperties().set ("bipolar", bipolar);
    k.repaint();
}

std::unique_ptr<ModuleUnit::Cell> ModuleUnit::makeKnobCell (juce::RangedAudioParameter& param, const juce::String& label, double defNorm, bool bipolar, int)
{
    auto c = std::make_unique<Cell>();
    c->label = label;
    c->knob = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
    auto& k = *c->knob;
    k.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    k.setMouseDragSensitivity (220);
    k.setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::ctrlModifier);   // Ctrl-drag = fine
    k.setScrollWheelEnabled (true);
    k.setPopupDisplayEnabled (true, true, nullptr, 900);
    c->knobAtt = std::make_unique<juce::SliderParameterAttachment> (param, k, &proc.undoManager);
    k.setDoubleClickReturnValue (true, param.convertFrom0to1 ((float) defNorm));
    auto* raw = &k; auto* pp = &param; auto name = label;
    k.onValueChange = [raw, pp, name] { raw->setTooltip (name + ": " + pp->getCurrentValueAsText()); };
    k.onValueChange();
    styleKnob (k, bipolar);
    addAndMakeVisible (k);
    return c;
}

void ModuleUnit::refresh()
{
    const int type = proc.getSlotType (slot);
    if (type == shownType) return;
    shownType = type;
    rebuild();
}

void ModuleUnit::rebuild()
{
    cells.clear();
    const auto* mi = ModuleRegistry::info (shownType);
    moduleBox.setSelectedId (shownType + 2, juce::dontSendNotification);
    osBox.setEnabled (mi != nullptr && mi->supportsOversampling);
    for (auto& t : trims) { t->knob->setEnabled (mi != nullptr); styleKnob (*t->knob, (bool) t->knob->getProperties()["bipolar"]); }
    power.setEnabled (mi != nullptr); soloButton.setEnabled (mi != nullptr);
    grBar.setVisible (mi != nullptr && mi->hasGainReduction);

    if (mi != nullptr)
    {
        for (size_t k = 0; k < mi->params.size(); ++k)
        {
            const auto& spec = mi->params[k];
            auto& param = *proc.slotParam (slot, "p" + juce::String ((int) k + 1));
            std::unique_ptr<Cell> c;
            if (spec.kind == ParamSpec::Kind::Choice)
            {
                c = std::make_unique<Cell>();
                c->label = spec.shortName;
                c->combo = std::make_unique<juce::ComboBox>();
                for (size_t i = 0; i < spec.choices.size(); ++i) c->combo->addItem (spec.choices[i], (int) i + 1);
                auto* box = c->combo.get();
                const int n = spec.numChoices();
                c->paramAtt = std::make_unique<juce::ParameterAttachment> (param, [box, n] (float v)
                    { box->setSelectedId ((int) std::lround (v * (n - 1)) + 1, juce::dontSendNotification); }, &proc.undoManager);
                auto* att = c->paramAtt.get();
                box->onChange = [box, att, n] { att->setValueAsCompleteGesture ((float) (box->getSelectedId() - 1) / (float) juce::jmax (1, n - 1)); };
                c->paramAtt->sendInitialUpdate();
                box->setTooltip (spec.name);
                addAndMakeVisible (*box);
            }
            else if (spec.kind == ParamSpec::Kind::Toggle)
            {
                c = std::make_unique<Cell>();
                c->label = spec.shortName;
                c->toggle = std::make_unique<juce::TextButton> ("OFF");
                auto* b = c->toggle.get();
                b->setClickingTogglesState (true);
                b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (mi->accent));
                c->paramAtt = std::make_unique<juce::ParameterAttachment> (param, [b] (float v)
                    { b->setToggleState (v > 0.5f, juce::dontSendNotification); b->setButtonText (v > 0.5f ? "ON" : "OFF"); }, &proc.undoManager);
                auto* att = c->paramAtt.get();
                b->onClick = [b, att] { att->setValueAsCompleteGesture (b->getToggleState() ? 1.0f : 0.0f); b->setButtonText (b->getToggleState() ? "ON" : "OFF"); };
                c->paramAtt->sendInitialUpdate();
                b->setTooltip (spec.name);
                addAndMakeVisible (*b);
            }
            else
            {
                const bool bipolar = spec.minV < 0 && spec.maxV > 0;
                c = makeKnobCell (param, spec.shortName, spec.defaultNorm(), bipolar, 46);
            }
            cells.push_back (std::move (c));
        }
    }
    resized();
    repaint();
}

void ModuleUnit::resized()
{
    moduleBox.setBounds (8, 8, kWidth - 38, 22);
    menuButton.setBounds (kWidth - 28, 8, 20, 22);
    power.setBounds (7, 33, 22, 22);
    soloButton.setBounds (30, 35, 18, 18);
    osBox.setBounds (kWidth - 50, 34, 42, 18);

    for (size_t i = 0; i < cells.size(); ++i)
    {
        const int col = (int) i % kCols, row = (int) i / kCols;
        auto area = juce::Rectangle<int> (8 + col * kCellW, kTop + row * kCellH, kCellW, kCellH);
        auto& c = *cells[i];
        c.area = area;
        auto body = area.withTrimmedBottom (13);
        if (c.knob)   c.knob->setBounds (body.withSizeKeepingCentre (48, 48));
        if (c.combo)  c.combo->setBounds (body.withSizeKeepingCentre (62, 20));
        if (c.toggle) c.toggle->setBounds (body.withSizeKeepingCentre (44, 20));
    }
    const int trimY = kTop + kRows * kCellH + 4;
    for (size_t i = 0; i < trims.size(); ++i)
    {
        auto area = juce::Rectangle<int> (6 + (int) i * 46, trimY, 46, 56);
        trims[i]->area = area;
        trims[i]->knob->setBounds (area.withTrimmedBottom (13).withSizeKeepingCentre (40, 40));
    }
    outBar.setBounds (12, trimY + 62, kWidth - 24, 8);
    grBar.setBounds (12, trimY + 74, kWidth - 24, 6);
}

void ModuleUnit::paint (juce::Graphics& g)
{
    const auto* mi = ModuleRegistry::info (shownType);
    const auto pc = plateColours (mi ? mi->faceplate : Faceplate::Graphite);
    auto r = getLocalBounds().toFloat();

    auto img = mi ? faceplateImage (mi->id) : juce::Image();
    if (img.isValid())
        g.drawImage (img, r, juce::RectanglePlacement::stretchToFit);
    else
    {
        const auto panel = mi ? pc.panel : juce::Colour (0xff1a1b1e);
        g.setGradientFill (juce::ColourGradient (panel.brighter (0.12f), 0, 0, panel.darker (0.25f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);
        // brushed texture
        juce::Random rng ((juce::int64) slot * 7919 + 17);
        for (int y = 0; y < kHeight; y += 2)
        {
            g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.018f));
            g.drawHorizontalLine (y, 2.0f, (float) kWidth - 2.0f);
        }
        if (mi)
        {
            g.setColour (juce::Colour (mi->accent).withAlpha (0.85f));
            g.fillRect (6.0f, 28.0f + 2.0f, r.getWidth() - 12.0f, 1.5f);
        }
    }
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

    // rack ears with screws
    for (float y : { 4.0f, (float) kHeight - 12.0f })
        for (float x : { 4.0f, (float) kWidth - 12.0f })
        {
            if (y < 10 && x < 10) continue;
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillEllipse (x, y, 8, 8);
            g.setColour (juce::Colour (0xffb8b8b8)); g.fillEllipse (x + 1, y + 1, 6, 6);
            g.setColour (juce::Colour (0xff555555)); g.drawLine (x + 2, y + 4, x + 6, y + 4, 1.0f);
        }

    // grip (drag handle)
    auto grip = gripArea().toFloat();
    g.setColour (pc.label.withAlpha (0.45f));
    for (int i = 0; i < 4; ++i)
        g.fillRoundedRectangle (grip.getX() + 6 + i * 9.0f, grip.getY() + 4, 3.0f, grip.getHeight() - 8, 1.0f);

    if (mi == nullptr)
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawFittedText ("EMPTY SLOT\n\nchoose a module\nfrom the list above", getLocalBounds().withTrimmedTop (120).withHeight (120), juce::Justification::centred, 5);
    }

    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.setColour (pc.label);
    for (auto& c : cells)
        g.drawFittedText (c->label, c->area.withTop (c->area.getBottom() - 13), juce::Justification::centred, 1);
    g.setColour ((mi ? pc.label : juce::Colour (0xff9a9a9a)).withAlpha (mi ? 1.0f : 0.5f));
    for (auto& t : trims)
        g.drawFittedText (t->label, t->area.withTop (t->area.getBottom() - 13), juce::Justification::centred, 1);

    // module title + chain position
    if (mi)
    {
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.setColour (pc.text.withAlpha (0.9f));
        g.drawText (juce::String (mi->name).toUpperCase(), 0, kHeight - 30, kWidth, 14, juce::Justification::centred);
    }
    g.setFont (juce::FontOptions (9.0f));
    g.setColour (pc.text.withAlpha (0.5f));
    const int pos = getPosition ? getPosition() : slot;
    g.drawText ("SLOT " + juce::String (slot + 1) + "  /  POS " + juce::String (pos + 1), 0, kHeight - 17, kWidth, 12, juce::Justification::centred);
}

void ModuleUnit::paintOverChildren (juce::Graphics& g)
{
    if (proc.slotParam (slot, "bypass")->getValue() > 0.5f && shownType >= 0)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().withTrimmedTop (56), 4.0f);
    }
    if (dragOver)
    {
        g.setColour (juce::Colour (0xffe0a040));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.5f), 4.0f, 3.0f);
    }
}

void ModuleUnit::updateMeters()
{
    auto& s = proc.engine.slot (slot);
    outBar.setLevel (s.takePeak());
    if (grBar.isVisible()) grBar.setGr (s.getGrDb());
    static thread_local int tick = 0;
    if ((++tick & 7) == 0) repaint (0, 56, kWidth, kHeight - 56);   // bypass shading follows automation
}

//==============================================================================
void ModuleUnit::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (gripArea().contains (e.getPosition()) ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void ModuleUnit::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) showMenu();
}

void ModuleUnit::mouseDrag (const juce::MouseEvent& e)
{
    if (! gripArea().contains (e.getMouseDownPosition()) || e.getDistanceFromDragStart() < 4) return;
    if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! dnd->isDragAndDropActive())
        {
            auto snapshot = createComponentSnapshot (getLocalBounds()).rescaled (kWidth / 2, kHeight / 2);
            juce::Image faded (juce::Image::ARGB, snapshot.getWidth(), snapshot.getHeight(), true);
            { juce::Graphics g (faded); g.setOpacity (0.7f); g.drawImageAt (snapshot, 0, 0); }
            dnd->startDragging ("slot:" + juce::String (slot), this, juce::ScaledImage (faded), true);
        }
}

bool ModuleUnit::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("slot:") && d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue() != slot;
}

void ModuleUnit::itemDropped (const SourceDetails& d)
{
    dragOver = false;
    const int src = d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
    proc.undoManager.beginNewTransaction ("Move module");
    proc.moveSlotToPosition (src, getPosition ? getPosition() : slot);
    if (auto* parent = getParentComponent()) parent->resized();
    repaint();
}

//==============================================================================
void ModuleUnit::showMenu()
{
    juce::PopupMenu m;
    const bool has = shownType >= 0;
    m.addItem (1, "Save module preset...", has);
    m.addItem (2, "Load module preset...");
    m.addSeparator();
    m.addItem (3, "Copy module", has);
    m.addItem (4, "Paste module", proc.hasClipboard());
    m.addSeparator();
    m.addItem (5, "Reset to defaults", has);
    m.addItem (6, "Clear slot", has);

    juce::Component::SafePointer<ModuleUnit> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& self = *safe;
        auto& p = self.proc;
        p.undoManager.beginNewTransaction();
        if (r == 1 || r == 2)
        {
            moduleFolder().createDirectory();
            const bool save = r == 1;
            const auto* mi = ModuleRegistry::info (self.shownType);
            auto start = moduleFolder().getChildFile (save && mi ? juce::String (mi->name) + ".blmod" : juce::String());
            self.chooser = std::make_unique<juce::FileChooser> (save ? "Save module preset" : "Load module preset", start, kModuleExt);
            const int flags = juce::FileBrowserComponent::canSelectFiles
                            | (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                                    : juce::FileBrowserComponent::openMode);
            self.chooser->launchAsync (flags, [safe, save] (const juce::FileChooser& fc)
            {
                if (safe == nullptr) return;
                auto f = fc.getResult();
                if (f == juce::File()) return;
                if (save)
                {
                    if (auto xml = safe->proc.makeModuleState (safe->slot).createXml())
                        xml->writeTo (f.withFileExtension ("blmod"));
                }
                else if (auto xml = juce::XmlDocument::parse (f))
                {
                    safe->proc.loadModuleState (safe->slot, juce::ValueTree::fromXml (*xml));
                    safe->refresh();
                }
            });
        }
        else if (r == 3) p.copyModule (self.slot);
        else if (r == 4) { p.pasteModule (self.slot); self.refresh(); }
        else if (r == 5) p.resetSlotParams (self.slot);
        else if (r == 6) { p.setSlotType (self.slot, -1, true); self.refresh(); }
    });
}
} // namespace bl
