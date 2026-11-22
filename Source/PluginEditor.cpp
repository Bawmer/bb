#include "PluginEditor.h"
#include "Presets/FactoryPresets.h"

using namespace bl;

namespace
{
const juce::Colour kBg (0xff0f1012), kBar (0xff17181c), kAccent (0xffe0a040), kText (0xffd9dadd);
constexpr int kInitId = 1, kFactoryBase = 100, kUserBase = 1000;

void styleSmallKnob (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    s.setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::ctrlModifier);
    s.setPopupDisplayEnabled (true, true, nullptr, 900);
    s.getProperties().set ("plate", (int) Faceplate::Graphite);
    s.getProperties().set ("accent", (juce::int64) 0xffe0a040);
    s.getProperties().set ("bipolar", true);
}
}

//==============================================================================
RackContent::RackContent (MixRackProcessor& p) : proc (p)
{
    setSize (kWidth, kHeight);
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
        grStrip[(size_t) s] = std::make_unique<LevelBar> (true, true);
        addAndMakeVisible (*grStrip[(size_t) s]);
    }

    presetBox.setTextWhenNothingSelected ("Init");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        const auto& fp = factoryPresets();
        if (id == kInitId) proc.loadInitRack();
        else if (id >= kUserBase && id - kUserBase < (int) userPresets.size()) proc.loadUserPreset (userPresets[(size_t) (id - kUserBase)]);
        else if (id >= kFactoryBase && id - kFactoryBase < (int) fp.size()) proc.applyRackPreset (fp[(size_t) (id - kFactoryBase)]);
        for (auto& u : units) u->refresh();
        resized();
    };
    presetBox.setTooltip ("Rack presets");
    addAndMakeVisible (presetBox);
    buildPresetMenu();

    auto step = [this] (int dir)
    {
        juce::Array<int> ids;
        ids.add (kInitId);
        for (int i = 0; i < (int) factoryPresets().size(); ++i) ids.add (kFactoryBase + i);
        for (int i = 0; i < (int) userPresets.size(); ++i) ids.add (kUserBase + i);
        int idx = ids.indexOf (presetBox.getSelectedId());
        idx = (idx + dir + ids.size()) % ids.size();
        presetBox.setSelectedId (ids[idx], juce::sendNotificationSync);
    };
    prevButton.onClick = [step] { step (-1); };
    nextButton.onClick = [step] { step (1); };
    saveButton.setTooltip ("Save the whole rack as a user preset (.blrack)");
    saveButton.onClick = [this]
    {
        auto dir = MixRackProcessor::userPresetFolder();
        dir.createDirectory();
        chooser = std::make_unique<juce::FileChooser> ("Save rack preset", dir.getChildFile ("My Rack.blrack"), "*.blrack");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File()) return;
                proc.saveUserPreset (f.withFileExtension ("blrack"));
                buildPresetMenu();
            });
    };

    aButton.setClickingTogglesState (false);
    aButton.onClick = [this] { proc.selectAB (0); refreshAB(); for (auto& u : units) u->refresh(); resized(); };
    bButton.onClick = [this] { proc.selectAB (1); refreshAB(); for (auto& u : units) u->refresh(); resized(); };
    copyAB.setTooltip ("Copy the current setting to the other A/B slot");
    copyAB.onClick = [this] { proc.copyCurrentToOther(); };
    aButton.setTooltip ("Compare: setting A"); bButton.setTooltip ("Compare: setting B");
    undoButton.onClick = [this] { proc.undoManager.undo(); for (auto& u : units) u->refresh(); };
    redoButton.onClick = [this] { proc.undoManager.redo(); for (auto& u : units) u->refresh(); };
    refreshAB();

    for (auto* b : { &prevButton, &nextButton, &saveButton, &aButton, &bButton, &copyAB, &undoButton, &redoButton })
        addAndMakeVisible (*b);

    styleSmallKnob (inGain); styleSmallKnob (outGain);
    inAtt  = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("in_gain"), inGain, &proc.undoManager);
    outAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("out_gain"), outGain, &proc.undoManager);
    inGain.setTooltip ("Global input gain"); outGain.setTooltip ("Global output gain");
    addAndMakeVisible (inGain); addAndMakeVisible (outGain);

    osModeBox.addItemList ({ "OS: Per Slot", "OS: Off", "OS: 2x All", "OS: 4x All" }, 1);
    osAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*proc.apvts.getParameter ("os_mode"), osModeBox, &proc.undoManager);
    osModeBox.setTooltip ("Global oversampling override for nonlinear modules");
    addAndMakeVisible (osModeBox);

    for (auto* m : { &inL, &inR, &outL, &outR }) addAndMakeVisible (*m);

    addMouseListener (this, true);
    resized();
    startTimerHz (30);
}

RackContent::~RackContent() { removeMouseListener (this); stopTimer(); }

void RackContent::mouseDown (const juce::MouseEvent&)
{
    proc.undoManager.beginNewTransaction();
}

void RackContent::refreshAB()
{
    aButton.setToggleState (proc.getActiveAB() == 0, juce::dontSendNotification);
    bButton.setToggleState (proc.getActiveAB() == 1, juce::dontSendNotification);
}

void RackContent::buildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    auto* root = presetBox.getRootMenu();
    root->addItem (kInitId, "Init");
    const auto& fp = factoryPresets();
    for (auto* bank : factoryBanks())
    {
        juce::PopupMenu sub;
        for (int i = 0; i < (int) fp.size(); ++i)
            if (juce::String (fp[(size_t) i].bank) == bank) sub.addItem (kFactoryBase + i, fp[(size_t) i].name);
        root->addSubMenu (bank, sub);
    }
    userPresets.clear();
    auto dir = MixRackProcessor::userPresetFolder();
    if (dir.isDirectory())
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.blrack"))
            userPresets.push_back (f);
    std::sort (userPresets.begin(), userPresets.end(), [] (auto& a, auto& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    juce::PopupMenu user;
    for (int i = 0; i < (int) userPresets.size(); ++i) user.addItem (kUserBase + i, userPresets[(size_t) i].getFileNameWithoutExtension());
    if (userPresets.empty()) user.addItem (-1, "(no user presets yet)", false);
    root->addSeparator();
    root->addSubMenu ("User", user);
    presetBox.setText (proc.currentPresetName, juce::dontSendNotification);
}

void RackContent::timerCallback()
{
    const auto order = proc.getOrder();
    if (order != shownOrder) { shownOrder = order; resized(); for (auto& u : units) u->repaint(); }
    for (auto& u : units) { u->refresh(); u->updateMeters(); }

    inL.setLevel (proc.inMeter.takePeak (0), proc.inMeter.getRms (0));
    inR.setLevel (proc.inMeter.takePeak (1), proc.inMeter.getRms (1));
    outL.setLevel (proc.outMeter.takePeak (0), proc.outMeter.getRms (0));
    outR.setLevel (proc.outMeter.takePeak (1), proc.outMeter.getRms (1));
    for (int s = 0; s < kNumSlots; ++s) grStrip[(size_t) s]->setGr (proc.engine.slot (s).getGrDb());

    const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    const int lat = proc.getLatencySamples();
    const float lufs = proc.outMeter.getLufsS();
    auto newLat = "LATENCY " + juce::String (lat) + " smp (" + juce::String (lat * 1000.0 / fs, 1) + " ms)";
    auto newCpu = "CPU " + juce::String (juce::jlimit (0.0f, 999.0f, proc.cpuLoad.load() * 100.0f), 1) + " %";
    auto newLufs = lufs > -99.0f ? juce::String (lufs, 1) : juce::String ("-inf");
    if (newLat != latencyText || newCpu != cpuText || newLufs != lufsText)
    {
        latencyText = newLat; cpuText = newCpu; lufsText = newLufs;
        repaint (0, 0, getWidth(), kTopH);
        repaint (0, getHeight() - kBottomH, getWidth(), kBottomH);
    }
    if (presetBox.getText() != proc.currentPresetName && ! presetBox.isPopupActive())
        presetBox.setText (proc.currentPresetName, juce::dontSendNotification);
    refreshAB();
}

void RackContent::resized()
{
    if (units[0] == nullptr) return;
    // top bar
    int x = 172;
    presetBox.setBounds (x, 14, 280, 26); x += 284;
    prevButton.setBounds (x, 14, 24, 26); x += 26;
    nextButton.setBounds (x, 14, 24, 26); x += 30;
    saveButton.setBounds (x, 14, 52, 26); x += 64;
    aButton.setBounds (x, 14, 28, 26); x += 30;
    bButton.setBounds (x, 14, 28, 26); x += 30;
    copyAB.setBounds (x, 14, 40, 26); x += 52;
    undoButton.setBounds (x, 14, 52, 26); x += 54;
    redoButton.setBounds (x, 14, 52, 26); x += 64;
    inGain.setBounds (x, 4, 40, 40); x += 64;
    outGain.setBounds (x, 4, 40, 40); x += 66;
    osModeBox.setBounds (x, 14, 110, 26);

    // rack units in chain order
    for (int pos = 0; pos < kNumSlots; ++pos)
    {
        const int s = shownOrder[(size_t) pos];
        units[(size_t) s]->setTopLeftPosition (kMargin + pos * (ModuleUnit::kWidth + kGap), kTopH);
    }

    // bottom bar
    const int by = kTopH + ModuleUnit::kHeight + 14;
    inL.setBounds (52, by + 14, 220, 9);  inR.setBounds (52, by + 26, 220, 9);
    outL.setBounds (342, by + 14, 220, 9); outR.setBounds (342, by + 26, 220, 9);
    for (int pos = 0; pos < kNumSlots; ++pos)
    {
        const int s = shownOrder[(size_t) pos];
        grStrip[(size_t) s]->setBounds (760 + pos * 56, by + 12, 10, 40);
    }
}

void RackContent::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    // rack rails
    auto rail = juce::Rectangle<int> (0, kTopH - 6, getWidth(), ModuleUnit::kHeight + 12);
    g.setColour (juce::Colour (0xff08090a)); g.fillRect (rail);
    g.setColour (juce::Colour (0xff26282d));
    for (int x = 4; x < getWidth(); x += 16) { g.fillEllipse ((float) x, (float) kTopH - 5, 4, 4); g.fillEllipse ((float) x, (float) (kTopH + ModuleUnit::kHeight + 2), 4, 4); }

    // top bar
    auto top = juce::Rectangle<int> (0, 0, getWidth(), kTopH - 6);
    g.setGradientFill (juce::ColourGradient (kBar.brighter (0.1f), 0, 0, kBar.darker (0.3f), 0, (float) top.getBottom(), false));
    g.fillRect (top);
    g.setColour (kAccent);
    g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    g.drawText ("BUB LEON", 16, 6, 150, 22, juce::Justification::left);
    g.setColour (kText.withAlpha (0.8f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("MIX RACK  |  BL AUDIO", 16, 27, 160, 14, juce::Justification::left);

    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    g.setColour (kText.withAlpha (0.6f));
    g.drawText ("IN", inGain.getX() - 16, 18, 14, 12, juce::Justification::right);
    g.drawText ("OUT", outGain.getX() - 22, 18, 20, 12, juce::Justification::right);

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (kText.withAlpha (0.75f));
    g.drawText (latencyText, getWidth() - 200, 8, 188, 16, juce::Justification::right);
    g.drawText (cpuText, getWidth() - 200, 26, 188, 16, juce::Justification::right);

    // bottom bar
    const int by = kTopH + ModuleUnit::kHeight + 8;
    auto bottom = juce::Rectangle<int> (0, by, getWidth(), getHeight() - by);
    g.setColour (kBar); g.fillRect (bottom);
    g.setColour (kText.withAlpha (0.65f));
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("INPUT",  14, by + 18, 36, 12, juce::Justification::left);
    g.drawText ("OUTPUT", 290, by + 18, 50, 12, juce::Justification::left);
    g.setFont (juce::FontOptions (9.0f));
    for (int db : { -48, -36, -24, -12, -6, 0 })
    {
        const float f = (db + 60.0f) / 66.0f;
        g.drawText (juce::String (db), 52 + (int) (220 * f) - 12, by + 38, 24, 10, juce::Justification::centred);
        g.drawText (juce::String (db), 342 + (int) (220 * f) - 12, by + 38, 24, 10, juce::Justification::centred);
    }
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("LUFS-S", 590, by + 12, 60, 12, juce::Justification::left);
    g.setColour (kAccent);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText (lufsText, 590, by + 26, 120, 28, juce::Justification::left);

    g.setColour (kText.withAlpha (0.65f));
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("GAIN REDUCTION (dB, chain order)", 760 - 16, by - 1, 400, 12, juce::Justification::left);
    g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
    for (int pos = 0; pos < kNumSlots; ++pos)
    {
        const auto* mi = ModuleRegistry::info (proc.getSlotType (shownOrder[(size_t) pos]));
        g.drawText (mi ? juce::String (mi->shortName) : juce::String ("-"), 760 + pos * 56 - 20, by + 58, 50, 12, juce::Justification::centred);
    }
}

//==============================================================================
MixRackEditor::MixRackEditor (MixRackProcessor& p) : AudioProcessorEditor (p), proc (p), content (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (RackContent::kWidth, RackContent::kHeight, RackContent::kWidth * 2, RackContent::kHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) RackContent::kWidth / RackContent::kHeight);
    const double scale = juce::jlimit (1.0, 2.0, (double) proc.apvts.state.getProperty ("uiScale", 1.0));
    setSize ((int) (RackContent::kWidth * scale), (int) (RackContent::kHeight * scale));
}

MixRackEditor::~MixRackEditor()
{
    setLookAndFeel (nullptr);
}

void MixRackEditor::resized()
{
    const float scale = (float) getWidth() / (float) RackContent::kWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    proc.apvts.state.setProperty ("uiScale", scale, nullptr);
}
