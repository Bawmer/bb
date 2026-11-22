#include "GUI/ModuleUnit.h"
#include "PluginProcessor.h"
#include <map>

namespace bl
{
namespace
{
// unit geometry taken from the reference artwork (219 x 603)
constexpr int kColX[2] = { 57, 160 };
constexpr int kRowY[3] = { 106, 214, 316 };           // big knob centres (image layout)
constexpr int kRowYCompact[3] = { 98, 192, 284 };     // when a second row of small controls is needed
constexpr int kSmallRowY = 399, kSmallRowYCompact[2] = { 366, 416 };
constexpr int kTrimY = 483, kTrimX[3] = { 37, 107, 175 };
const juce::Rectangle<int> kVu { 42, 539, 134, 42 };
const char* kModuleExt = "*.blmod";
enum MenuIds { kSave = 901, kLoad, kCopy, kPaste, kReset, kClear };

juce::File moduleFolder() { return MixRackProcessor::userPresetFolder().getChildFile ("Modules"); }

void shadowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Colour col, bool light)
{
    if (! light) { g.setColour (juce::Colours::black.withAlpha (0.65f)); g.drawText (t, r.translated (0.8f, 1.4f), juce::Justification::centred); }
    else         { g.setColour (juce::Colours::white.withAlpha (0.35f)); g.drawText (t, r.translated (0.0f, 1.0f), juce::Justification::centred); }
    g.setColour (col);
    g.drawText (t, r, juce::Justification::centred);
}

void engraved (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Colour col)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));  g.drawText (t, r.translated (0, -1.0f), juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.10f)); g.drawText (t, r.translated (0, 1.2f), juce::Justification::centred);
    g.setColour (col);                                     g.drawText (t, r, juce::Justification::centred);
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

    osLamp.setClickingTogglesState (false);
    osLamp.setTooltip ("Oversampling for this module (click: 1x / 2x / 4x)");
    osLamp.onClick = [this] { showOsMenu(); };
    addAndMakeVisible (osLamp);

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
    KnobStyle st = KnobStyle::Chrome;
    if (mi && (mi->faceplate == Faceplate::Silver || mi->faceplate == Faceplate::Cream
                    || mi->faceplate == Faceplate::Blue || mi->faceplate == Faceplate::Bronze || mi->faceplate == Faceplate::Hammer))
        st = KnobStyle::Gold;
    k.getProperties().set ("knob", (int) st);
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
    if (trim) k.getProperties().set ("trim", true);
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
        if (led && safe != nullptr) safe->repaint (raw->getBounds().getCentreX() - 16, raw->getY() - 14, 32, 32);
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

    power.setVisible (has); solo.setVisible (has); osLamp.setVisible (has); vu.setVisible (has);
    for (auto& t : trims) { t->knob->setVisible (has); styleKnob (*t->knob); }
    osLamp.setEnabled (has && mi->supportsOversampling);
    if (has) power.setLedColour (juce::Colour (0xffff3a22));   // red power lamp, as on the hardware

    if (has)
    {
        // continuous controls take the big knob positions (max 6); the rest go to the small-control row
        std::vector<int> big, small;
        for (int k = 0; k < (int) mi->params.size(); ++k)
            (! mi->params[(size_t) k].isDiscrete() && big.size() < 6 ? big : small).push_back (k);

        auto addCell = [&] (int k, bool isSmall)
        {
            const auto& spec = mi->params[(size_t) k];
            auto& param = *proc.slotParam (slot, "p" + juce::String (k + 1));
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
            {
                c = makeKnobCell (param, spec.shortName, spec.defaultNorm(), false);
                if (isSmall) { c->knob->getProperties().set ("trim", true); c->hasLed = false; styleKnob (*c->knob); }
            }
            c->small = isSmall;
            cells.push_back (std::move (c));
        };
        for (int k : big) addCell (k, false);
        for (int k : small) addCell (k, true);
    }
    resized();
    repaint();
}

void ModuleUnit::resized()
{
    power.setBounds (juce::Rectangle<int> (42, 42).withCentre ({ 34, 29 }));

    int nSmall = 0;
    for (auto& c : cells) nSmall += c->small ? 1 : 0;
    compact = nSmall > 3;
    const int bigSize = compact ? 84 : 96;
    const int smallRows = compact ? 2 : 1;
    const int perRow = juce::jmax (1, (nSmall + smallRows - 1) / smallRows);

    int bigIdx = 0, smallIdx = 0;
    for (auto& cp : cells)
    {
        auto& c = *cp;
        if (! c.small)
        {
            const int cx = kColX[bigIdx % 2], cy = (compact ? kRowYCompact : kRowY)[juce::jmin (2, bigIdx / 2)];
            c.area = juce::Rectangle<int> (bigSize, bigSize).withCentre ({ cx, cy });
            c.knob->setBounds (c.area);
            c.labelPos = { cx, cy + (compact ? 45 : 50) };
            ++bigIdx;
        }
        else
        {
            const int row = smallIdx / perRow, col = smallIdx % perRow;
            const int inRow = juce::jmin (perRow, nSmall - row * perRow);
            const float span = (float) (kWidth - 40), x0 = 20.0f;
            const int cx = (int) (x0 + span * (col + 0.5f) / (float) inRow);
            const int cy = compact ? kSmallRowYCompact[row] : kSmallRowY;
            const int cw = (int) (span / (float) inRow);
            c.area = juce::Rectangle<int> (cw, 40).withCentre ({ cx, cy });
            if (c.knob)   c.knob->setBounds (juce::Rectangle<int> (40, 40).withCentre ({ cx, cy - 2 }));
            if (c.combo)  c.combo->setBounds (juce::Rectangle<int> (juce::jmin (62, cw - 4), 20).withCentre ({ cx, cy - 2 }));
            if (c.toggle) c.toggle->setBounds (juce::Rectangle<int> (22, 27).withCentre ({ cx, cy - 3 }));
            c.labelPos = { cx, cy + 22 };
            ++smallIdx;
        }
    }
    for (size_t i = 0; i < trims.size(); ++i)
    {
        trims[i]->area = juce::Rectangle<int> (70, 70).withCentre ({ kTrimX[i], kTrimY });
        trims[i]->knob->setBounds (trims[i]->area);
        trims[i]->labelPos = { kTrimX[i], kTrimY + 38 };
    }
    vu.setBounds (kVu);
    solo.setBounds (juce::Rectangle<int> (30, 30).withCentre ({ 27, 557 }));
    osLamp.setBounds (juce::Rectangle<int> (30, 30).withCentre ({ 192, 557 }));
}

void ModuleUnit::paint (juce::Graphics& g)
{
    const auto* mi = ModuleRegistry::info (shownType);
    const auto plate = mi ? mi->faceplate : Faceplate::Black;
    const bool light = mi && skin::isLight (plate);
    auto r = getLocalBounds().toFloat();

    // photographic faceplate (or the user's own PNG); corner screws are part of the plate
    auto custom = mi ? faceplateImage (mi->id) : juce::Image();
    g.setOpacity (1.0f);
    g.drawImage (custom.isValid() ? custom : (mi ? skin::plate (plate) : skin::emptyPlate()), r, juce::RectanglePlacement::stretchToFit);

    const auto ink = light ? skin::labelDk : skin::labelLt;
    if (mi == nullptr)
    {
        const auto c = juce::Colour (0xffc9c5bd);
        g.setFont (skin::sans (25.0f, false)); engraved (g, "- Empty -", { 0, 218, r.getWidth(), 30 }, c);
        g.setFont (skin::sans (25.0f, false)); engraved (g, "EMPTY SLOT", { 0, 258, r.getWidth(), 30 }, c);
        g.setFont (skin::sans (18.0f, true));
        shadowText (g, "choose a module", { 0, 294, r.getWidth(), 22 }, juce::Colour (0xfff2f0ea), false);
        shadowText (g, "from the list above", { 0, 318, r.getWidth(), 22 }, juce::Colour (0xfff2f0ea), false);
        return;
    }

    // title
    g.setFont (skin::sans (21.0f, true));
    {
        const auto t = titleArea().toFloat();
        if (! light) { g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawFittedText (mi->name, t.translated (0.8f, 1.5f).toNearestInt(), juce::Justification::centred, 1, 0.7f); }
        g.setColour (ink);
        g.drawFittedText (mi->name, t.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }

    // value LEDs above the big knobs + labels
    int bigIdx = 0;
    for (auto& c : cells)
    {
        if (c->hasLed && c->knob && ! c->small)
        {
            const auto kb = c->knob->getBounds();
            const auto col = bigIdx < 2 ? juce::Colour (0xffff3020) : juce::Colour (0xffffc23a);   // red top row, amber below
            skin::drawLed (g, { (float) kb.getCentreX(), (float) kb.getY() + (compact ? 3.0f : 3.0f) }, 4.2f, col, 0.2f + 0.8f * c->param->getValue());
        }
        if (! c->small) ++bigIdx;
        g.setFont (skin::sans (c->small ? 11.5f : (compact ? 14.5f : 16.5f), true));
        shadowText (g, c->label, juce::Rectangle<float> (c->small ? (float) c->area.getWidth() + 8 : 110.0f, 18.0f).withCentre (c->labelPos.toFloat()), ink, light);
    }
    g.setFont (skin::sans (16.0f, true));
    for (auto& t : trims)
        shadowText (g, t->label, juce::Rectangle<float> (70, 18).withCentre (t->labelPos.toFloat()), ink, light);

    // small captions around the VU
    auto* osP = proc.slotParam (slot, "os");
    const int os = (int) std::lround (osP->convertFrom0to1 (osP->getValue()));
    g.setFont (skin::sans (9.5f, true));
    g.setColour (ink.withAlpha (0.8f));
    g.drawText ("SOLO", juce::Rectangle<int> (4, 570, 46, 11), juce::Justification::centred);
    g.drawText (mi->supportsOversampling ? "OS " + juce::String (1 << os) + "x" : "OS -", juce::Rectangle<int> (169, 570, 46, 11), juce::Justification::centred);
    const int pos = getPosition ? getPosition() : slot;
    g.setFont (skin::sans (12.5f, true));
    shadowText (g, "SLOT " + juce::String (slot + 1) + " / POS " + juce::String (pos + 1), { 40, 582, 139, 16 }, ink.withAlpha (0.85f), light);
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
    if (isEmpty()) return;
    vu.pushLevel (proc.engine.slot (slot).takePeak());
    auto* osP = proc.slotParam (slot, "os");
    const bool osOn = osLamp.isEnabled() && osP->getValue() > 0.01f;
    if (osLamp.getToggleState() != osOn) { osLamp.setToggleState (osOn, juce::dontSendNotification); repaint (160, 560, 59, 24); }
    if (++repaintTick % 8 == 0) repaint (0, 50, kWidth, kHeight - 120);   // bypass shading follows automation
}

void ModuleUnit::showOsMenu()
{
    auto* osP = proc.slotParam (slot, "os");
    const int cur = (int) std::lround (osP->convertFrom0to1 (osP->getValue()));
    juce::PopupMenu m;
    m.addSectionHeader ("Oversampling");
    for (int i = 0; i < 3; ++i) m.addItem (i + 1, juce::String (1 << i) + "x", true, i == cur);
    juce::Component::SafePointer<ModuleUnit> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&osLamp), [safe, osP] (int r)
    {
        if (safe == nullptr || r == 0) return;
        osP->beginChangeGesture();
        osP->setValueNotifyingHost (osP->convertTo0to1 ((float) (r - 1)));
        osP->endChangeGesture();
        safe->repaint();
    });
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
