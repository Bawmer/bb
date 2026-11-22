#include "PluginEditor.h"
#include "Presets/FactoryPresets.h"

using namespace bl;

namespace
{
constexpr int kInitId = 1, kFactoryBase = 100, kUserBase = 1000;

// artwork coordinates (2032 x 1120 reference design)
constexpr float kUnitX0 = 123.0f, kUnitStep = 223.43f;
constexpr int kUnitY = 229;
const juce::Rectangle<int> kPresetLcd { 577, 80, 374, 86 };
const juce::Rectangle<int> kOsLcd { 1618, 81, 198, 78 };
const juce::Rectangle<int> kLufsLcd { 1245, 890, 202, 66 };
constexpr float kGrX0 = 500.0f, kGrStep = 190.0f;

void styleTopKnob (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    s.setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::ctrlModifier);
    s.setPopupDisplayEnabled (true, true, nullptr, 900);
    s.getProperties().set ("knob", (int) KnobStyle::Top);
    s.getProperties().set ("ticks", false);   // the tick ring is printed on the panel
}
}

//==============================================================================
void PresetDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    skin::drawAmberLcd (g, r);
    g.setColour (skin::lcdInk);
    g.setFont (skin::mono (25.0f));
    g.drawText ("PRESET:", r.reduced (16, 8).withHeight (30), juce::Justification::left);
    g.setFont (skin::mono (28.0f));
    g.drawFittedText (name, r.reduced (16, 8).withTrimmedTop (34).withTrimmedRight (26).toNearestInt(), juce::Justification::left, 1, 0.6f);
    // step arrows
    auto ar = r.removeFromRight (26).reduced (5, 12);
    juce::Path up, dn;
    up.addTriangle (ar.getX(), ar.getY() + 12, ar.getRight(), ar.getY() + 12, ar.getCentreX(), ar.getY());
    dn.addTriangle (ar.getX(), ar.getBottom() - 12, ar.getRight(), ar.getBottom() - 12, ar.getCentreX(), ar.getBottom());
    g.setColour (skin::lcdInk.withAlpha (0.55f));
    g.fillPath (up); g.fillPath (dn);
}

void PresetDisplay::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onMenu) onMenu(); return; }
    if (e.x > getWidth() - 30) { if (onStep) onStep (e.y < getHeight() / 2 ? -1 : 1); return; }
    if (onClick) onClick();
}

void PresetDisplay::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (onStep && std::abs (w.deltaY) > 0.0f) onStep (w.deltaY > 0 ? -1 : 1);
}

//==============================================================================
RackContent::RackContent (MixRackProcessor& p) : proc (p), bg (skin::background())
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    shownOrder = proc.getOrder();

    for (int s = 0; s < kNumSlots; ++s)
    {
        units[(size_t) s] = std::make_unique<ModuleUnit> (proc, s);
        units[(size_t) s]->getPosition = [this, s]
        {
            for (int pos = 0; pos < kNumSlots; ++pos) if (shownOrder[(size_t) pos] == s) return pos;
            return s;
        };
        addAndMakeVisible (*units[(size_t) s]);
        grLeds[(size_t) s] = std::make_unique<GrLeds>();
        addAndMakeVisible (*grLeds[(size_t) s]);
    }

    presetDisplay.setTooltip ("Click: presets  |  arrows / mouse wheel: previous / next  |  right-click: more");
    presetDisplay.onClick = [this]
    {
        auto m = buildPresetMenu();
        juce::Component::SafePointer<RackContent> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetDisplay), [safe] (int id) { if (safe != nullptr && id > 0) safe->loadPresetId (id); });
    };
    presetDisplay.onStep = [this] (int d) { stepPreset (d); };
    presetDisplay.onMenu = [this] { showPresetExtras(); };
    addAndMakeVisible (presetDisplay);

    saveButton.setTooltip ("Save the whole rack as a user preset (.blrack)");
    saveButton.onClick = [this] { savePreset(); };
    aButton.setTooltip ("Compare: setting A");
    bButton.setTooltip ("Compare: setting B");
    copyAB.setTooltip ("Copy the current setting to the other A/B memory");
    aButton.onClick = [this] { proc.selectAB (0); refreshAll(); };
    bButton.onClick = [this] { proc.selectAB (1); refreshAll(); };
    copyAB.onClick = [this] { proc.copyCurrentToOther(); };
    for (auto* b : { &saveButton, &aButton, &bButton, &copyAB }) addAndMakeVisible (*b);
    refreshAB();

    styleTopKnob (inGain); styleTopKnob (outGain);
    inAtt  = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("in_gain"), inGain, &proc.undoManager);
    outAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("out_gain"), outGain, &proc.undoManager);
    inGain.setDoubleClickReturnValue (true, 0.0); outGain.setDoubleClickReturnValue (true, 0.0);
    inGain.setTooltip ("Global input gain"); outGain.setTooltip ("Global output gain");
    addAndMakeVisible (inGain); addAndMakeVisible (outGain);

    addAndMakeVisible (inVu); addAndMakeVisible (outVu);
    inVu.setInterceptsMouseClicks (false, false); outVu.setInterceptsMouseClicks (false, false);

    setSize (kWidth, kHeight);
    addMouseListener (this, true);
    resized();
    startTimerHz (30);
}

RackContent::~RackContent() { removeMouseListener (this); stopTimer(); }

void RackContent::mouseDown (const juce::MouseEvent&) { proc.undoManager.beginNewTransaction(); }

void RackContent::mouseUp (const juce::MouseEvent& e)
{
    if (e.eventComponent == this && kOsLcd.contains (e.getPosition())) showOsMenu();
}

bool RackContent::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { proc.undoManager.undo(); refreshAll(); return true; }
    if (k == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0)
        || k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { proc.undoManager.redo(); refreshAll(); return true; }
    return false;
}

void RackContent::refreshAll()
{
    refreshAB();
    for (auto& u : units) u->refresh();
    resized();
    repaint();
}

void RackContent::refreshAB()
{
    aButton.setToggleState (proc.getActiveAB() == 0, juce::dontSendNotification);
    bButton.setToggleState (proc.getActiveAB() == 1, juce::dontSendNotification);
}

//==============================================================================
juce::PopupMenu RackContent::buildPresetMenu()
{
    juce::PopupMenu root;
    root.addItem (kInitId, "Init", true, currentPresetId == kInitId);
    const auto& fp = factoryPresets();
    for (auto* bank : factoryBanks())
    {
        juce::PopupMenu sub;
        for (int i = 0; i < (int) fp.size(); ++i)
            if (juce::String (fp[(size_t) i].bank) == bank) sub.addItem (kFactoryBase + i, fp[(size_t) i].name, true, currentPresetId == kFactoryBase + i);
        root.addSubMenu (bank, sub);
    }
    userPresets.clear();
    auto dir = MixRackProcessor::userPresetFolder();
    if (dir.isDirectory())
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.blrack"))
            userPresets.push_back (f);
    std::sort (userPresets.begin(), userPresets.end(), [] (auto& a, auto& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    juce::PopupMenu user;
    for (int i = 0; i < (int) userPresets.size(); ++i) user.addItem (kUserBase + i, userPresets[(size_t) i].getFileNameWithoutExtension(), true, currentPresetId == kUserBase + i);
    if (userPresets.empty()) user.addItem (-1, "(no user presets yet)", false);
    root.addSeparator();
    root.addSubMenu ("User", user);
    return root;
}

void RackContent::loadPresetId (int id)
{
    const auto& fp = factoryPresets();
    if (id == kInitId) proc.loadInitRack();
    else if (id >= kUserBase && id - kUserBase < (int) userPresets.size()) proc.loadUserPreset (userPresets[(size_t) (id - kUserBase)]);
    else if (id >= kFactoryBase && id - kFactoryBase < (int) fp.size()) proc.applyRackPreset (fp[(size_t) (id - kFactoryBase)]);
    else return;
    currentPresetId = id;
    refreshAll();
}

void RackContent::stepPreset (int dir)
{
    buildPresetMenu();   // refresh the user preset list
    juce::Array<int> ids;
    ids.add (kInitId);
    for (int i = 0; i < (int) factoryPresets().size(); ++i) ids.add (kFactoryBase + i);
    for (int i = 0; i < (int) userPresets.size(); ++i) ids.add (kUserBase + i);
    int idx = juce::jmax (0, ids.indexOf (currentPresetId));
    idx = (idx + dir + ids.size()) % ids.size();
    loadPresetId (ids[idx]);
}

void RackContent::savePreset()
{
    auto dir = MixRackProcessor::userPresetFolder();
    dir.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save rack preset", dir.getChildFile ("My Rack.blrack"), "*.blrack");
    juce::Component::SafePointer<RackContent> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            auto f = fc.getResult();
            if (f == juce::File()) return;
            safe->proc.saveUserPreset (f.withFileExtension ("blrack"));
            safe->presetDisplay.repaint();
        });
}

void RackContent::showPresetExtras()
{
    juce::PopupMenu m;
    m.addItem (1, "Previous preset");
    m.addItem (2, "Next preset");
    m.addItem (3, "Save rack preset...");
    m.addItem (4, "Init rack");
    m.addSeparator();
    m.addItem (5, "Undo   (Ctrl+Z)", proc.undoManager.canUndo());
    m.addItem (6, "Redo   (Ctrl+Y)", proc.undoManager.canRedo());
    m.addSeparator();
    m.addItem (7, "Open preset folder");
    juce::Component::SafePointer<RackContent> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetDisplay), [safe] (int r)
    {
        if (safe == nullptr) return;
        switch (r)
        {
            case 1: safe->stepPreset (-1); break;
            case 2: safe->stepPreset (1); break;
            case 3: safe->savePreset(); break;
            case 4: safe->loadPresetId (kInitId); break;
            case 5: safe->proc.undoManager.undo(); safe->refreshAll(); break;
            case 6: safe->proc.undoManager.redo(); safe->refreshAll(); break;
            case 7: MixRackProcessor::userPresetFolder().createDirectory(); MixRackProcessor::userPresetFolder().startAsProcess(); break;
            default: break;
        }
    });
}

void RackContent::showOsMenu()
{
    auto* prm = proc.apvts.getParameter ("os_mode");
    const int cur = (int) std::lround (prm->convertFrom0to1 (prm->getValue()));
    juce::PopupMenu m;
    m.addSectionHeader ("Oversampling (nonlinear modules)");
    const char* names[] = { "Per Slot", "Off", "2x All", "4x All" };
    for (int i = 0; i < 4; ++i) m.addItem (i + 1, names[i], true, i == cur);
    juce::Component::SafePointer<RackContent> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (kOsLcd)), [safe, prm] (int r)
    {
        if (safe == nullptr || r == 0) return;
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (r - 1)));
        prm->endChangeGesture();
        safe->repaint (kOsLcd);
    });
}

//==============================================================================
void RackContent::timerCallback()
{
    const auto order = proc.getOrder();
    if (order != shownOrder) { shownOrder = order; resized(); for (auto& u : units) u->repaint(); repaint(); }
    for (auto& u : units) { u->refresh(); u->updateMeters(); }

    inVu.pushLevel (juce::jmax (proc.inMeter.takePeak (0), proc.inMeter.takePeak (1)));
    outVu.pushLevel (juce::jmax (proc.outMeter.takePeak (0), proc.outMeter.takePeak (1)));
    for (int s = 0; s < kNumSlots; ++s) grLeds[(size_t) s]->setGr (proc.engine.slot (s).getGrDb());

    const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    const int lat = proc.getLatencySamples();
    const float lufs = proc.outMeter.getLufsS();
    auto newLat = "LATENCY " + juce::String (lat) + " smp (" + juce::String (lat * 1000.0 / fs, 1) + " ms)";
    auto newCpu = juce::String (juce::jlimit (0.0f, 999.0f, proc.cpuLoad.load() * 100.0f), 1) + "%";
    auto newLufs = lufs > -99.0f ? juce::String (juce::jlimit (-99.9f, 9.9f, lufs), 1) : juce::String ("--.-");
    if (newLat != latencyText) { latencyText = newLat; repaint (1590, 46, 280, 36); }
    if (newCpu != cpuText) { cpuText = newCpu; repaint (kOsLcd); }
    if (newLufs != lufsText) { lufsText = newLufs; repaint (kLufsLcd); }
    if (presetDisplay.name != proc.currentPresetName) { presetDisplay.name = proc.currentPresetName; presetDisplay.repaint(); }
    refreshAB();
}

juce::Rectangle<int> RackContent::unitRect (int pos) const
{
    return { (int) std::lround (kUnitX0 + pos * kUnitStep), kUnitY, ModuleUnit::kWidth, ModuleUnit::kHeight };
}

void RackContent::resized()
{
    if (units[0] == nullptr) return;

    presetDisplay.setBounds (kPresetLcd);
    saveButton.setBounds (1000, 102, 74, 41);
    aButton.setBounds (1088, 102, 62, 41);
    bButton.setBounds (1166, 102, 62, 41);
    copyAB.setBounds (1243, 102, 70, 41);
    inGain.setBounds (juce::Rectangle<int> (65, 65).withCentre ({ 1373, 120 }));
    outGain.setBounds (juce::Rectangle<int> (65, 65).withCentre ({ 1481, 120 }));

    for (int pos = 0; pos < kNumSlots; ++pos)
        units[(size_t) shownOrder[(size_t) pos]]->setBounds (unitRect (pos));

    inVu.setBounds (446, 879, 180, 67);    // printed VU faces; only the needles are live
    outVu.setBounds (653, 879, 180, 67);
    for (int pos = 0; pos < kNumSlots; ++pos)
        grLeds[(size_t) shownOrder[(size_t) pos]]->setBounds ((int) (kGrX0 + pos * kGrStep) - 70, 1034, 140, 24);
}

void RackContent::paint (juce::Graphics& g)
{
    g.setOpacity (1.0f);
    g.drawImage (bg, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

    // latency readout (top right)
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.setFont (skin::sans (19.0f, true));
    g.drawText (latencyText, juce::Rectangle<int> (1590, 49, 280, 30).translated (0, 1), juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.drawText (latencyText, juce::Rectangle<int> (1590, 49, 280, 30), juce::Justification::centred);

    // OS / CPU screen
    {
        auto r = kOsLcd.toFloat();
        skin::drawBlackLcd (g, r);
        auto* prm = proc.apvts.getParameter ("os_mode");
        const char* names[] = { "Per Slot", "Off", "2x All", "4x All" };
        const int os = juce::jlimit (0, 3, (int) std::lround (prm->convertFrom0to1 (prm->getValue())));
        g.setColour (skin::amber);
        g.setFont (skin::mono (27.0f));
        g.drawText ("OS: " + juce::String (names[os]), r.reduced (12, 6).withHeight (34), juce::Justification::left);
        g.drawText ("CPU", r.reduced (12, 6).withTrimmedTop (36), juce::Justification::left);
        g.drawText (cpuText, r.reduced (12, 6).withTrimmedTop (36), juce::Justification::right);
    }

    // loudness caption + 7-segment readout
    {
        const auto cap = juce::Rectangle<float> (900, 896, 270, 46);
        g.setFont (skin::sans (34.0f, false));
        g.setColour (juce::Colours::white.withAlpha (0.35f)); g.drawText (juce::String::fromUTF8 ("\xe2\x80\x94 LUFS-S \xe2\x80\x94"), cap.translated (0, 1.5f), juce::Justification::centred);
        g.setColour (juce::Colour (0xff4a4a4c));              g.drawText (juce::String::fromUTF8 ("\xe2\x80\x94 LUFS-S \xe2\x80\x94"), cap, juce::Justification::centred);
        skin::drawBlackLcd (g, kLufsLcd.toFloat());
        skin::drawSevenSeg (g, lufsText, kLufsLcd.toFloat().reduced (14, 13), skin::amber);
    }

    // GR labels (chain order)
    g.setFont (skin::sans (18.0f, true));
    for (int pos = 0; pos < kNumSlots; ++pos)
    {
        const auto* mi = ModuleRegistry::info (proc.getSlotType (shownOrder[(size_t) pos]));
        g.setColour (mi ? juce::Colour (0xfff2dcb4) : juce::Colour (0x40f2dcb4));
        g.drawText (mi ? juce::String (mi->shortName) : juce::String ("-"), juce::Rectangle<float> (kGrX0 + pos * kGrStep - 80, 1060, 160, 22), juce::Justification::centred);
    }
}

//==============================================================================
MixRackEditor::MixRackEditor (MixRackProcessor& p) : AudioProcessorEditor (p), proc (p), content (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    setResizable (true, true);
    constexpr int minW = 1250, maxW = 2500;
    setResizeLimits (minW, minW * RackContent::kHeight / RackContent::kWidth, maxW, maxW * RackContent::kHeight / RackContent::kWidth);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) RackContent::kWidth / RackContent::kHeight);
    const double scale = juce::jlimit ((double) minW / RackContent::kWidth, (double) maxW / RackContent::kWidth,
                                       (double) proc.apvts.state.getProperty ("uiScale", 0.75));
    setSize ((int) std::lround (RackContent::kWidth * scale), (int) std::lround (RackContent::kHeight * scale));
}

MixRackEditor::~MixRackEditor() { setLookAndFeel (nullptr); }

void MixRackEditor::resized()
{
    const float scale = (float) getWidth() / (float) RackContent::kWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    proc.apvts.state.setProperty ("uiScale", scale, nullptr);
}
