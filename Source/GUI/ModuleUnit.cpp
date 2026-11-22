#include "GUI/ModuleUnit.h"
#include "PluginProcessor.h"
#include <map>

namespace bl
{
namespace
{
constexpr int kGridTop = 56, kGridBottom = 444, kTrimY = 452, kVuY = 540;
const char* kModuleExt = "*.blmod";
enum MenuIds { kSave = 901, kLoad, kCopy, kPaste, kReset, kClear };

juce::File moduleFolder() { return MixRackProcessor::userPresetFolder().getChildFile ("Modules"); }

void drawEngraved (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Font f, juce::Colour col)
{
    g.setFont (f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawText (t, r.translated (0, -1.0f), juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawText (t, r.translated (0, 1.2f), juce::Justification::centred);
    g.setColour (col);
    g.drawText (t, r, juce::Justification::centred);
}
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

    power.setTooltip ("Module on / bypass");
    powerAtt = std::make_unique<juce::ButtonParameterAttachment> (*proc.slotParam (slot, "bypass"), power, &proc.undoManager);
    addAndMakeVisible (power);

    solo.setTooltip ("Solo: listen to this module only (all others bypassed)");
    soloAtt = std::make_unique<juce::ButtonParameterAttachment> (*proc.slotParam (slot, "solo"), solo, &proc.undoManager);
    addAndMakeVisible (solo);

    osBox.addItemList ({ "1x", "2x", "4x" }, 1);
    osBox.setTooltip ("Oversampling for this module");
    osAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*proc.slotParam (slot, "os"), osBox, &proc.undoManager);
    addAndMakeVisible (osBox);

    addAndMakeVisible (vu);
    vu.setInterceptsMouseClicks (false, false);

    trims.push_back (makeKnobCell (*proc.slotParam (slot, "in"), "IN", 0.5, true));
    trims.push_back (makeKnobCell (*proc.slotParam (slot, "mix"), "MIX", 1.0, true));
    trims.push_back (makeKnobCell (*proc.slotParam (slot, "out"), "OUT", 0.5, true));

    refresh();
}

ModuleUnit::~ModuleUnit() = default;

void ModuleUnit::styleKnob (juce::Slider& k)
{
    const auto* mi = ModuleRegistry::info (shownType);
    k.getProperties().set ("knob", (int) KnobStyle::Chrome);
    k.getProperties().set ("lightPlate", mi != nullptr && skin::isLight (mi->faceplate));
    k.repaint();
}

std::unique_ptr<ModuleUnit::Cell> ModuleUnit::makeKnobCell (juce::RangedAudioParameter& param, const juce::String& label, double defNorm, bool trim)
{
    auto c = std::make_unique<Cell>();
    c->label = label;
    c->param = &param;
    c->hasLed = ! trim;
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
    juce::Component::SafePointer<ModuleUnit> safe (this);
    const bool led = c->hasLed;
    k.onValueChange = [raw, pp, name, safe, led]
    {
        raw->setTooltip (name + ": " + pp->getCurrentValueAsText());
        if (led && safe != nullptr) safe->repaint (raw->getBounds().translated (0, -12).withHeight (14));
    };
    k.onValueChange();
    styleKnob (k);
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
    const bool has = mi != nullptr;

    power.setVisible (has); solo.setVisible (has); osBox.setVisible (has); vu.setVisible (has);
    for (auto& t : trims) { t->knob->setVisible (has); styleKnob (*t->knob); }
    osBox.setEnabled (has && mi->supportsOversampling);
    if (has) power.setLedColour (juce::Colour (mi->accent).withSaturation (0.9f).withBrightness (1.0f));

    if (has)
    {
        for (size_t k = 0; k < mi->params.size(); ++k)
        {
            const auto& spec = mi->params[k];
            auto& param = *proc.slotParam (slot, "p" + juce::String ((int) k + 1));
            std::unique_ptr<Cell> c;
            if (spec.kind == ParamSpec::Kind::Choice)
            {
                c = std::make_unique<Cell>();
                c->label = spec.shortName; c->param = &param;
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
                c->label = spec.shortName; c->param = &param;
                c->toggle = std::make_unique<ToggleSwitch>();
                auto* b = c->toggle.get();
                c->paramAtt = std::make_unique<juce::ParameterAttachment> (param, [b] (float v)
                    { b->setToggleState (v > 0.5f, juce::dontSendNotification); }, &proc.undoManager);
                auto* att = c->paramAtt.get();
                b->onClick = [b, att] { att->setValueAsCompleteGesture (b->getToggleState() ? 1.0f : 0.0f); };
                c->paramAtt->sendInitialUpdate();
                b->setTooltip (spec.name + " (up = on)");
                addAndMakeVisible (*b);
            }
            else
                c = makeKnobCell (param, spec.shortName, spec.defaultNorm(), false);
            cells.push_back (std::move (c));
        }
    }
    resized();
    repaint();
}

void ModuleUnit::resized()
{
    power.setBounds (16, 16, 26, 26);

    const int n = (int) cells.size();
    gridCols = n <= 6 ? 2 : 3;
    const int rows = juce::jmax (1, (n + gridCols - 1) / gridCols);
    const int rowH = juce::jmin (116, (kGridBottom - kGridTop) / rows);
    const float colW = (kWidth - 16) / (float) gridCols;
    const int knob = juce::jmin (74, rowH - 28, (int) colW - 4);
    labelSize = gridCols == 2 ? 14 : 12;
    for (int i = 0; i < n; ++i)
    {
        const int col = i % gridCols, row = i / gridCols;
        auto area = juce::Rectangle<int> ((int) (8 + col * colW), kGridTop + row * rowH, (int) colW, rowH);
        auto& c = *cells[(size_t) i];
        c.area = area;
        auto body = juce::Rectangle<int> (area.getX(), area.getY() + 10, area.getWidth(), knob);
        if (c.knob)   c.knob->setBounds (body.withSizeKeepingCentre (knob, knob));
        if (c.combo)  c.combo->setBounds (body.withSizeKeepingCentre (juce::jmin (68, (int) colW - 6), 22));
        if (c.toggle) c.toggle->setBounds (body.withSizeKeepingCentre (22, 30));
    }
    for (size_t i = 0; i < trims.size(); ++i)
    {
        auto area = juce::Rectangle<int> (8 + (int) i * 68, kTrimY, 68, 78);
        trims[i]->area = area;
        trims[i]->knob->setBounds (area.withHeight (62).withSizeKeepingCentre (60, 60));
    }
    vu.setBounds (43, kVuY, 133, 42);
    solo.setBounds (10, kVuY + 4, 26, 26);
    osBox.setBounds (kWidth - 40, kVuY + 8, 32, 18);
}

void ModuleUnit::paint (juce::Graphics& g)
{
    const auto* mi = ModuleRegistry::info (shownType);
    const auto plate = mi ? mi->faceplate : Faceplate::Black;
    const bool light = mi && skin::isLight (plate);
    auto r = getLocalBounds().toFloat();

    // brushed faceplate (or the user's own PNG) with lighting
    auto custom = mi ? faceplateImage (mi->id) : juce::Image();
    g.drawImage (custom.isValid() ? custom : (mi ? skin::plate (plate) : skin::emptyPlate()), r, juce::RectanglePlacement::stretchToFit);
    if (! custom.isValid())
    {
        juce::ColourGradient light1 (juce::Colours::white.withAlpha (0.10f), r.getCentreX(), r.getY() + 120, juce::Colours::black.withAlpha (0.28f), r.getX(), r.getBottom(), true);
        g.setGradientFill (light1); g.fillRect (r);
    }
    g.setColour (juce::Colours::white.withAlpha (0.16f)); g.drawLine (1.0f, 1.0f, r.getRight() - 1.0f, 1.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawRect (r, 1.2f);

    for (auto pt : { juce::Point<float> (14, 14), juce::Point<float> (r.getRight() - 14, 14),
                     juce::Point<float> (14, r.getBottom() - 14), juce::Point<float> (r.getRight() - 14, r.getBottom() - 14) })
        skin::drawScrew (g, pt, 6.5f);

    const auto ink = light ? skin::labelDk : skin::labelLt;
    if (mi == nullptr)
    {
        const auto c = juce::Colour (0xffb9b6b0);
        drawEngraved (g, "- Empty -", { 0, 230, r.getWidth(), 30 }, skin::sans (22.0f, false), c);
        drawEngraved (g, "EMPTY SLOT", { 0, 266, r.getWidth(), 30 }, skin::sans (23.0f, false), c);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (skin::sans (16.5f, true));
        g.drawText ("choose a module", juce::Rectangle<float> (0, 304, r.getWidth(), 22), juce::Justification::centred);
        g.drawText ("from the list above", juce::Rectangle<float> (0, 328, r.getWidth(), 22), juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.setFont (skin::sans (11.0f, false));
        const int pos = getPosition ? getPosition() : slot;
        g.drawText ("click to load  |  slot " + juce::String (slot + 1) + " / pos " + juce::String (pos + 1), juce::Rectangle<float> (0, 560, r.getWidth(), 16), juce::Justification::centred);
        return;
    }

    // title (click = module menu, drag = move)
    g.setColour (juce::Colours::black.withAlpha (light ? 0.0f : 0.5f));
    g.setFont (skin::sans (18.0f, true));
    auto title = titleArea().toFloat();
    g.drawFittedText (mi->name, title.translated (0, 1.2f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    g.setColour (ink);
    g.drawFittedText (mi->name, title.toNearestInt(), juce::Justification::centred, 1, 0.7f);

    // control labels + value LEDs above knobs
    g.setFont (skin::sans ((float) labelSize, true));
    const auto accent = juce::Colour (mi->accent).withSaturation (0.85f).withBrightness (1.0f);
    for (auto& c : cells)
    {
        const int top = c->knob ? c->knob->getBottom() : (c->combo ? c->combo->getBottom() + 8 : c->toggle->getBottom());
        g.setColour (juce::Colours::black.withAlpha (light ? 0.0f : 0.45f));
        g.drawFittedText (c->label, juce::Rectangle<int> (c->area.getX(), top + 2, c->area.getWidth(), 16).translated (0, 1), juce::Justification::centred, 1, 0.7f);
        g.setColour (ink);
        g.drawFittedText (c->label, juce::Rectangle<int> (c->area.getX(), top + 2, c->area.getWidth(), 16), juce::Justification::centred, 1, 0.7f);
        if (c->hasLed && c->knob)
            skin::drawLed (g, { (float) c->knob->getBounds().getCentreX(), (float) c->knob->getY() - 4.0f }, 3.2f, accent,
                           0.2f + 0.8f * c->param->getValue());
    }
    g.setFont (skin::sans (14.0f, true));
    for (auto& t : trims)
    {
        g.setColour (ink);
        g.drawText (t->label, juce::Rectangle<int> (t->area.getX(), t->area.getY() + 60, t->area.getWidth(), 16), juce::Justification::centred);
    }
    g.setColour (ink.withAlpha (0.75f));
    g.setFont (skin::sans (9.5f, true));
    g.drawText ("SOLO", juce::Rectangle<int> (4, kVuY + 30, 38, 12), juce::Justification::centred);
    g.drawText ("OS", juce::Rectangle<int> (kWidth - 42, kVuY + 28, 36, 12), juce::Justification::centred);
    const int pos = getPosition ? getPosition() : slot;
    g.setColour (ink.withAlpha (0.55f));
    g.drawText ("SLOT " + juce::String (slot + 1) + " / POS " + juce::String (pos + 1), juce::Rectangle<int> (30, kHeight - 20, kWidth - 60, 12), juce::Justification::centred);
}

void ModuleUnit::paintOverChildren (juce::Graphics& g)
{
    if (! isEmpty() && proc.slotParam (slot, "bypass")->getValue() > 0.5f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRect (getLocalBounds().toFloat().withTrimmedTop (50).withTrimmedBottom (70).reduced (4, 0));
    }
    if (dragOver)
    {
        g.setColour (skin::amber);
        g.drawRect (getLocalBounds().toFloat().reduced (1.5f), 3.0f);
    }
}

void ModuleUnit::updateMeters()
{
    if (! isEmpty()) vu.pushLevel (proc.engine.slot (slot).takePeak());
    if (++repaintTick % 8 == 0 && ! isEmpty()) repaint (0, 50, kWidth, kHeight - 120);   // bypass shading follows automation
}

//==============================================================================
void ModuleUnit::mouseMove (const juce::MouseEvent& e)
{
    const bool overTitle = titleArea().contains (e.getPosition()) || isEmpty();
    setMouseCursor (overTitle ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void ModuleUnit::mouseDown (const juce::MouseEvent& e)
{
    dragStarted = false;
    if (e.mods.isPopupMenu()) showModuleMenu();
}

void ModuleUnit::mouseDrag (const juce::MouseEvent& e)
{
    if (isEmpty() || ! titleArea().contains (e.getMouseDownPosition()) || e.getDistanceFromDragStart() < 5) return;
    if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! dnd->isDragAndDropActive())
        {
            dragStarted = true;
            auto snapshot = createComponentSnapshot (getLocalBounds()).rescaled (kWidth / 2, kHeight / 2);
            juce::Image faded (juce::Image::ARGB, snapshot.getWidth(), snapshot.getHeight(), true);
            { juce::Graphics g (faded); g.setOpacity (0.7f); g.drawImageAt (snapshot, 0, 0); }
            dnd->startDragging ("slot:" + juce::String (slot), this, juce::ScaledImage (faded), true);
        }
}

void ModuleUnit::mouseUp (const juce::MouseEvent& e)
{
    if (dragStarted || e.mods.isPopupMenu() || e.getDistanceFromDragStart() > 4) return;
    if (titleArea().contains (e.getPosition()) || isEmpty()) showModuleMenu();
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
void ModuleUnit::showModuleMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Load module");
    m.addItem (1, "- Empty -", true, shownType < 0);
    for (auto cat : { "Color", "EQ", "Dynamics", "Utility" })
    {
        juce::PopupMenu sub;
        for (int t = 0; t < ModuleRegistry::count(); ++t)
            if (ModuleRegistry::info (t)->category == cat)
                sub.addItem (t + 2, ModuleRegistry::info (t)->name, true, t == shownType);
        m.addSubMenu (cat, sub);
    }
    const bool has = shownType >= 0;
    m.addSeparator();
    m.addSectionHeader ("Module");
    m.addItem (kSave, "Save module preset...", has);
    m.addItem (kLoad, "Load module preset...");
    m.addItem (kCopy, "Copy module", has);
    m.addItem (kPaste, "Paste module", proc.hasClipboard());
    m.addItem (kReset, "Reset to defaults", has);
    m.addItem (kClear, "Clear slot", has);
    m.addSeparator();
    m.addItem (-1, "Drag the title to move this module", false);

    juce::Component::SafePointer<ModuleUnit> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (titleArea())),
                     [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r >= kSave) { safe->runMenuAction (r); return; }
        const int type = r - 2;
        if (type != safe->proc.getSlotType (safe->slot))
        {
            safe->proc.undoManager.beginNewTransaction ("Change module");
            safe->proc.setSlotType (safe->slot, type, true);
            safe->refresh();
        }
    });
}

void ModuleUnit::runMenuAction (int r)
{
    auto& p = proc;
    p.undoManager.beginNewTransaction();
    juce::Component::SafePointer<ModuleUnit> safe (this);
    if (r == kSave || r == kLoad)
    {
        moduleFolder().createDirectory();
        const bool save = r == kSave;
        const auto* mi = ModuleRegistry::info (shownType);
        auto start = moduleFolder().getChildFile (save && mi ? juce::String (mi->name) + ".blmod" : juce::String());
        chooser = std::make_unique<juce::FileChooser> (save ? "Save module preset" : "Load module preset", start, kModuleExt);
        const int flags = juce::FileBrowserComponent::canSelectFiles
                        | (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                                : juce::FileBrowserComponent::openMode);
        chooser->launchAsync (flags, [safe, save] (const juce::FileChooser& fc)
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
    else if (r == kCopy)  p.copyModule (slot);
    else if (r == kPaste) { p.pasteModule (slot); refresh(); }
    else if (r == kReset) p.resetSlotParams (slot);
    else if (r == kClear) { p.setSlotType (slot, -1, true); refresh(); }
}
} // namespace bl
