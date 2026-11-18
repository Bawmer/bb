#include "PluginEditor.h"

namespace
{
using labels::midiToName;
using labels::intervalLabel;

juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}
}

//==============================================================================
HarmonyLookAndFeel::HarmonyLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, ui::panelHi);
    setColour (juce::ComboBox::textColourId, ui::text);
    setColour (juce::ComboBox::outlineColourId, ui::goldDim);
    setColour (juce::ComboBox::arrowColourId, ui::gold);
    setColour (juce::PopupMenu::backgroundColourId, ui::panel);
    setColour (juce::PopupMenu::textColourId, ui::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::goldDim);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, ui::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, ui::text);
    setColour (juce::TextEditor::highlightColourId, ui::goldDim);
}

juce::Label* HarmonyLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (12.0f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textColourId, ui::text);
    l->setColour (juce::Label::outlineWhenEditingColourId, ui::gold);
    l->setColour (juce::TextEditor::backgroundColourId, ui::panelHi);
    l->setColour (juce::TextEditor::textColourId, ui::text);
    return l;
}

void HarmonyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float start, float end, juce::Slider& s)
{
    auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size = juce::jmin (b.getWidth(), b.getHeight());
    b = b.withSizeKeepingCentre (size, size);
    const auto c = b.getCentre();
    const float r = size * 0.5f;
    const float ang = start + pos * (end - start);

    // outer track
    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2, r - 2, 0, start, end, true);
    g.setColour (juce::Colour (0xff252e4d));
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc (bipolar sliders start at centre)
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? start + (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) * (end - start) : start;
    juce::Path val;
    val.addCentredArc (c.x, c.y, r - 2, r - 2, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
    g.setColour (accent);
    g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob body
    const float kr = r * 0.72f;
    juce::ColourGradient grad (juce::Colour (0xff2f3a60), c.x, c.y - kr, juce::Colour (0xff121830), c.x, c.y + kr, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.2f);

    // pointer
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.6f, -kr + 3.0f, 3.2f, kr * 0.45f, 1.5f);
    g.setColour (accent.brighter (0.2f));
    g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void HarmonyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hl, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);
    if (b.getButtonText().isEmpty())
    {
        // round power button
        const float s = juce::jmin (r.getWidth(), r.getHeight());
        auto c = r.withSizeKeepingCentre (s, s);
        g.setColour (on ? accent.withAlpha (0.25f) : ui::panelHi);
        g.fillEllipse (c);
        g.setColour (on ? accent : ui::textDim);
        g.drawEllipse (c.reduced (1.0f), 1.5f);
        juce::Path p;
        const float cr = s * 0.24f;
        p.addCentredArc (c.getCentreX(), c.getCentreY(), cr, cr, 0, 0.7f, 6.283f - 0.7f, true);
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (c.getCentreX(), c.getCentreY() - cr - 1.5f, c.getCentreX(), c.getCentreY() - 1.0f, 1.8f);
        return;
    }
    g.setColour (on ? accent.withAlpha (0.22f) : ui::panelHi.withAlpha (hl ? 1.0f : 0.8f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? accent : ui::goldDim.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 6.0f, 1.3f);
    g.setColour (on ? accent : ui::textDim);
    g.setFont (font (13.0f, true));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void HarmonyLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId).withAlpha (box.hasKeyboardFocus (true) ? 1.0f : 0.6f));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 5, ay - 2.5f, ax + 5, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void HarmonyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool hl, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool filled = b.getProperties()["filled"];
    auto c = base;
    if (! b.isEnabled()) c = c.withMultipliedAlpha (0.4f);
    if (filled)
    {
        g.setColour (c.brighter (down ? -0.15f : (hl ? 0.12f : 0.0f)));
        g.fillRoundedRectangle (r, 7.0f);
    }
    else
    {
        g.setColour (c.withAlpha (hl ? 0.22f : 0.12f));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (c.withAlpha (0.8f));
        g.drawRoundedRectangle (r, 7.0f, 1.3f);
    }
}

void HarmonyLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool filled = b.getProperties()["filled"];
    g.setColour (filled ? juce::Colour (0xff141018) : b.findColour (juce::TextButton::buttonColourId).brighter (0.3f));
    if (! b.isEnabled()) g.setOpacity (0.5f);
    g.setFont (font (filled ? 16.0f : 13.0f, true));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

//==============================================================================
DragChip::DragChip (juce::String l, juce::StringArray p, juce::Colour a)
    : label (std::move (l)), paths (std::move (p)), accent (a)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Drag into your DAW\n" + paths.joinIntoString ("\n"));
}

void DragChip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (accent.withAlpha (hover ? 0.28f : 0.14f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (accent.withAlpha (hover ? 1.0f : 0.7f));
    g.drawRoundedRectangle (r, 8.0f, 1.3f);
    // grip dots
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 2; ++col)
            g.fillEllipse (r.getX() + 9.0f + col * 5.0f, r.getCentreY() - 7.0f + row * 6.0f, 2.6f, 2.6f);
    // mini waveform
    juce::Random rnd ((juce::int64) label.hashCode());
    const float wx = r.getRight() - 34.0f;
    for (int i = 0; i < 9; ++i)
    {
        const float h = 4.0f + rnd.nextFloat() * 14.0f;
        g.fillRoundedRectangle (wx + i * 3.0f, r.getCentreY() - h * 0.5f, 1.8f, h, 0.9f);
    }
    g.setColour (ui::text);
    g.setFont (font (12.5f, true));
    g.drawFittedText (label, r.withTrimmedLeft (24).withTrimmedRight (38).toNearestInt(), juce::Justification::centredLeft, 2);
}

void DragChip::mouseDown (const juce::MouseEvent&) { dragging = false; }

void DragChip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 5) return;
    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles (paths, false, this);
}

//==============================================================================
void StemZone::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (dragHover ? ui::gold.withAlpha (0.18f) : ui::bg.withAlpha (0.7f));
    g.fillRoundedRectangle (r, 8.0f);
    juce::Path border;
    border.addRoundedRectangle (r, 8.0f);
    if (overview.empty())
    {
        juce::Path dashed;
        const float dash[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, border, dash, 2);
        g.setColour (dragHover ? ui::gold : ui::textDim.withAlpha (0.6f));
        g.fillPath (dashed);
        g.setFont (font (13.0f, true));
        g.setColour (dragHover ? ui::gold : ui::text);
        g.drawText ("DROP VOCAL STEM HERE", r.withTrimmedBottom (r.getHeight() * 0.4f), juce::Justification::centredBottom);
        g.setFont (font (11.0f));
        g.setColour (ui::textDim);
        g.drawText ("or click to browse  -  WAV / AIFF / FLAC / MP3", r.withTrimmedTop (r.getHeight() * 0.55f), juce::Justification::centredTop);
        return;
    }
    g.setColour (dragHover ? ui::gold : ui::goldDim.withAlpha (0.6f));
    g.strokePath (border, juce::PathStrokeType (1.2f));
    auto wave = r.reduced (8.0f, 6.0f).withTrimmedTop (14.0f);
    const float mid = wave.getCentreY();
    float mx = 1.0e-6f;
    for (float v : overview) mx = juce::jmax (mx, v);
    g.setColour (ui::ice.withAlpha (0.75f));
    const int n = (int) overview.size();
    for (int i = 0; i < n; ++i)
    {
        const float x = wave.getX() + wave.getWidth() * (float) i / (float) n;
        const float h = juce::jmax (1.0f, overview[(size_t) i] / mx * wave.getHeight() * 0.5f);
        g.fillRect (x, mid - h, juce::jmax (1.0f, wave.getWidth() / (float) n - 0.3f), h * 2.0f);
    }
    g.setColour (ui::text);
    g.setFont (font (11.5f, true));
    g.drawText (name, r.reduced (8.0f, 3.0f).removeFromTop (14.0f), juce::Justification::centredLeft);
    g.setColour (ui::textDim);
    g.drawText (info, r.reduced (8.0f, 3.0f).removeFromTop (14.0f), juce::Justification::centredRight);
}

//==============================================================================
void PitchKeyboard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ui::panel);
    g.fillRoundedRectangle (r, 8.0f);
    r = r.reduced (8.0f, 6.0f);

    auto isBlack = [] (int n) { const int k = n % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; };
    int whites = 0;
    for (int n = lowNote; n <= highNote; ++n) if (! isBlack (n)) ++whites;
    const float ww = r.getWidth() / (float) whites;

    auto colourFor = [this] (int n, juce::Colour& c) -> bool
    {
        for (int v = 3; v >= 0; --v)
            if (voiceMidi[v] >= 0.0f && (int) std::lround (voiceMidi[v]) == n) { c = ui::voiceCol[v]; return true; }
        if (inputMidi >= 0.0f && (int) std::lround (inputMidi) == n) { c = juce::Colours::white; return true; }
        return false;
    };

    // white keys
    std::vector<float> whiteX;
    int wi = 0;
    for (int n = lowNote; n <= highNote; ++n)
    {
        if (isBlack (n)) continue;
        auto k = juce::Rectangle<float> (r.getX() + wi * ww, r.getY(), ww, r.getHeight()).reduced (0.8f, 0);
        juce::Colour c;
        const bool lit = colourFor (n, c);
        g.setColour (lit ? c : juce::Colour (0xffc9cfdf));
        g.fillRoundedRectangle (k, 2.5f);
        if (lit && c == juce::Colours::white)
        {
            g.setColour (ui::ice);
            g.drawRoundedRectangle (k.reduced (1.0f), 2.5f, 2.0f);
        }
        if (n % 12 == 0)
        {
            g.setColour (juce::Colour (0xff59617d));
            g.setFont (font (10.0f));
            g.drawText ("C" + juce::String (n / 12 - 1), k.removeFromBottom (14.0f), juce::Justification::centred);
        }
        ++wi;
    }
    // black keys
    wi = 0;
    for (int n = lowNote; n <= highNote; ++n)
    {
        if (! isBlack (n)) { ++wi; continue; }
        const float bw = ww * 0.62f;
        auto k = juce::Rectangle<float> (r.getX() + wi * ww - bw * 0.5f, r.getY(), bw, r.getHeight() * 0.6f);
        juce::Colour c;
        const bool lit = colourFor (n, c);
        g.setColour (lit ? c : juce::Colour (0xff1a1f30));
        g.fillRoundedRectangle (k, 2.0f);
        if (lit && c == juce::Colours::white) { g.setColour (ui::ice); g.drawRoundedRectangle (k, 2.0f, 2.0f); }
    }

    // continuous pitch cursor
    if (inputMidi >= lowNote && inputMidi <= highNote)
    {
        // map midi -> x using white-key geometry
        auto xFor = [&] (float m)
        {
            int wcount = 0;
            const int base = (int) std::floor (m);
            for (int n = lowNote; n < base; ++n) if (! isBlack (n)) ++wcount;
            float x = r.getX() + wcount * ww + (isBlack (base) ? 0.0f : ww * 0.5f);
            return x + (m - (float) base) * ww * 0.58f;
        };
        const float x = xFor (inputMidi);
        g.setColour (ui::ice.withAlpha (0.9f));
        g.fillRect (x - 1.0f, r.getY() - 4.0f, 2.0f, r.getHeight() + 8.0f);
    }
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label, juce::Colour a, bool small)
    : caption (label), accent (a)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, small ? 15 : 17);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, ui::text);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    slider.setDoubleClickReturnValue (true, s.getParameter (id)->convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (caption.isEmpty() ? 0 : 15);
    slider.setBounds (r);
}

void Knob::paint (juce::Graphics& g)
{
    if (caption.isEmpty()) return;
    g.setColour (ui::textDim);
    g.setFont (font (11.0f, true));
    g.drawText (caption.toUpperCase(), getLocalBounds().removeFromTop (14), juce::Justification::centred);
}

//==============================================================================
VoiceStrip::VoiceStrip (juce::AudioProcessorValueTreeState& s, int i)
    : index (i),
      interval (s, ID::vInt (i), "", ui::voiceCol[i]),
      level (s, ID::vLevel (i), "Level", ui::voiceCol[i], true),
      pan (s, ID::vPan (i), "Pan", ui::voiceCol[i], true),
      detune (s, ID::vDetune (i), "Detune", ui::voiceCol[i], true),
      state (s)
{
    power.setColour (juce::ToggleButton::tickColourId, ui::voiceCol[i]);
    addAndMakeVisible (power);
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, ID::vOn (i), power);
    interval.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    interval.slider.setMouseDragSensitivity (120);
    for (auto* k : { &interval, &level, &pan, &detune }) addAndMakeVisible (*k);
}

void VoiceStrip::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto head = r.removeFromTop (28);
    power.setBounds (head.removeFromLeft (28));
    r.removeFromTop (4);
    auto big = r.removeFromTop (104);
    interval.setBounds (big.withSizeKeepingCentre (104, 104));
    r.removeFromTop (24);
    const int w = r.getWidth() / 3;
    level.setBounds (r.removeFromLeft (w));
    pan.setBounds (r.removeFromLeft (w));
    detune.setBounds (r);
}

void VoiceStrip::refresh (int mode, int scale, float targetMidi, int heldNotes)
{
    const int val = (int) std::lround (interval.slider.getValue());
    auto newName = intervalLabel (mode, scale, val, index);
    if (mode == 2 && index >= heldNotes) newName = "waiting for MIDI";
    auto newNote = midiToName (targetMidi);
    if (newName != intervalName || newNote != noteName)
    {
        intervalName = newName; noteName = newNote;
        repaint();
    }
    interval.setEnabled (mode != 2);
    interval.setAlpha (mode == 2 ? 0.35f : 1.0f);
}

void VoiceStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const bool on = power.getToggleState();
    g.setColour (ui::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (ui::voiceCol[index].withAlpha (on ? 0.55f : 0.15f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.2f);
    g.fillRoundedRectangle (r.removeFromTop (3.0f).reduced (14.0f, 0), 1.5f);

    auto inner = getLocalBounds().reduced (10);
    auto head = inner.removeFromTop (28);
    head.removeFromLeft (36);
    g.setFont (font (14.0f, true));
    g.setColour (on ? ui::text : ui::textDim);
    g.drawText ("VOICE " + juce::String (index + 1), head, juce::Justification::centredLeft);
    g.setColour (on ? ui::voiceCol[index] : ui::textDim);
    g.setFont (font (18.0f, true));
    g.drawText (on ? noteName : "OFF", head, juce::Justification::centredRight);

    auto lbl = getLocalBounds().reduced (10).withTrimmedTop (28 + 4 + 104).removeFromTop (22);
    g.setFont (font (14.0f, true));
    g.setColour (on ? ui::voiceCol[index] : ui::textDim);
    g.drawText (intervalName, lbl, juce::Justification::centred);
}

//==============================================================================
HarmonyStackEditor::HarmonyStackEditor (HarmonyStackProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      humanize (p.apvts, ID::humanize, "Humanize"),
      width (p.apvts, ID::dblWidth, "Width"),
      formant (p.apvts, ID::formant, "Formant", ui::ice),
      breath (p.apvts, ID::breath, "Breath", ui::ice),
      glide (p.apvts, ID::glide, "Glide", ui::ice),
      vib (p.apvts, ID::vibDepth, "Vibrato", ui::ice),
      vibRate (p.apvts, ID::vibRate, "Vib Rate", ui::ice),
      gate (p.apvts, ID::gate, "Gate", ui::ice),
      dry (p.apvts, ID::dry, "Dry"),
      harmony (p.apvts, ID::harmony, "Harmony"),
      output (p.apvts, ID::output, "Output")
{
    setLookAndFeel (&lnf);

    modeBox.addItemList ({ "Scale", "Chromatic", "MIDI" }, 1);
    keyBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    for (int i = 0; i < hs::kNumScales; ++i) scaleBox.addItem (hs::scaleTable()[i].name, i + 1);
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };

    for (auto* b : { &modeBox, &keyBox, &scaleBox, &presetBox }) addAndMakeVisible (*b);
    modeAtt  = std::make_unique<ComboAtt> (proc.apvts, ID::mode, modeBox);
    keyAtt   = std::make_unique<ComboAtt> (proc.apvts, ID::key, keyBox);
    scaleAtt = std::make_unique<ComboAtt> (proc.apvts, ID::scale, scaleBox);

    addAndMakeVisible (keyboard);
    for (int i = 0; i < 4; ++i)
    {
        strips[i] = std::make_unique<VoiceStrip> (proc.apvts, i);
        addAndMakeVisible (*strips[i]);
    }

    doubleBtn.setColour (juce::ToggleButton::tickColourId, ui::gold);
    addAndMakeVisible (doubleBtn);
    doubleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ID::dbl, doubleBtn);
    for (auto* k : { &humanize, &width, &formant, &breath, &glide, &vib, &vibRate, &gate, &dry, &harmony, &output })
        addAndMakeVisible (*k);

    // ---- Auto Harmony panel ----
    addAndMakeVisible (stemZone);
    stemZone.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a vocal stem", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) loadStemFile (fc.getResult()); });
    };
    captureBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffff5a5a));
    captureBtn.setTooltip ("Arm, then press play in your DAW: the vocal is recorded while the transport runs");
    captureBtn.onClick = [this] { proc.armCapture (! proc.isCaptureArmed()); };
    generateBtn.setColour (juce::TextButton::buttonColourId, ui::gold);
    generateBtn.getProperties().set ("filled", true);
    generateBtn.onClick = [this]
    {
        if (proc.renderer.isBusy()) { proc.cancelGenerate(); return; }
        proc.setPreviewPlaying (false);
        juce::String err;
        if (! proc.startGenerate (err)) statusText = err;
    };
    previewBtn.setColour (juce::TextButton::buttonColourId, ui::ice);
    previewBtn.setTooltip ("Play the generated Lead + Stack through the plugin output");
    previewBtn.onClick = [this] { proc.setPreviewPlaying (! proc.isPreviewPlaying()); };
    folderBtn.setColour (juce::TextButton::buttonColourId, ui::textDim);
    folderBtn.onClick = [this]
    {
        auto f = proc.lastResult.folder;
        if (! f.isDirectory()) f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Bub Leon Harmony Stack");
        f.createDirectory();
        f.startAsProcess();
    };
    for (auto* b : { &autoKeyBtn, &allStylesBtn, &stemsBtn }) b->setColour (juce::ToggleButton::tickColourId, ui::ice);
    autoKeyBtn.setTooltip ("Detect the song key from the vocal and set Key/Scale automatically (Scale mode)");
    allStylesBtn.setTooltip ("Render a harmony stack for every style preset in one go");
    stemsBtn.setTooltip ("Also render each voice as its own file");
    autoKeyBtn.setToggleState (proc.autoKey, juce::dontSendNotification);
    allStylesBtn.setToggleState (proc.allStyles, juce::dontSendNotification);
    stemsBtn.setToggleState (proc.voiceStems, juce::dontSendNotification);
    autoKeyBtn.onClick   = [this] { proc.autoKey = autoKeyBtn.getToggleState(); };
    allStylesBtn.onClick = [this] { proc.allStyles = allStylesBtn.getToggleState(); };
    stemsBtn.onClick     = [this] { proc.voiceStems = stemsBtn.getToggleState(); };
    for (auto* b : std::initializer_list<juce::Component*> { &captureBtn, &generateBtn, &previewBtn, &folderBtn, &autoKeyBtn, &allStylesBtn, &stemsBtn })
        addAndMakeVisible (b);
    chipView.setViewedComponent (&chipHolder, false);
    chipView.setScrollBarsShown (false, true);
    chipView.setScrollBarThickness (6);
    addAndMakeVisible (chipView);

    sendLookAndFeelChange();   // children were created before the LookAndFeel was attached
    setSize (1000, 860);
    startTimerHz (30);
}

HarmonyStackEditor::~HarmonyStackEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void HarmonyStackEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    juce::ColourGradient glow (ui::panelHi.withAlpha (0.6f), 0, 0, ui::bg, 0, 260, false);
    g.setGradientFill (glow);
    g.fillRect (getLocalBounds().removeFromTop (260));

    // title
    g.setColour (ui::gold);
    g.setFont (font (13.0f, true));
    g.drawText ("B U B   L E O N", 24, 14, 300, 16, juce::Justification::left);
    g.setColour (ui::text);
    g.setFont (font (28.0f, true));
    g.drawText ("HARMONY STACK", 22, 28, 360, 34, juce::Justification::left);

    // labels for selectors
    g.setColour (ui::textDim);
    g.setFont (font (11.0f, true));
    g.drawText ("MODE", modeBox.getBounds().translated (0, -16).withHeight (14), juce::Justification::left);
    g.drawText ("KEY", keyBox.getBounds().translated (0, -16).withHeight (14), juce::Justification::left);
    g.drawText ("SCALE", scaleBox.getBounds().translated (0, -16).withHeight (14), juce::Justification::left);
    g.drawText ("PRESET", presetBox.getBounds().translated (0, -16).withHeight (14), juce::Justification::left);

    // pitch readout
    auto pr = juce::Rectangle<float> ((float) getWidth() - 214.0f, 14.0f, 190.0f, 60.0f);
    g.setColour (ui::panel);
    g.fillRoundedRectangle (pr, 8.0f);
    g.setColour (voiced ? ui::ice : ui::textDim.withAlpha (0.4f));
    g.drawRoundedRectangle (pr, 8.0f, 1.2f);
    g.fillEllipse (pr.getX() + 12, pr.getY() + 12, 8, 8);
    g.setFont (font (10.0f, true));
    g.setColour (ui::textDim);
    g.drawText ("INPUT PITCH", pr.withTrimmedLeft (26).withHeight (32), juce::Justification::centredLeft);
    g.setColour (voiced ? juce::Colours::white : ui::textDim);
    g.setFont (font (26.0f, true));
    g.drawText (pitchText, pr.reduced (12, 4).withTrimmedTop (18), juce::Justification::centredLeft);

    // output meter
    auto mr = pr.withX (pr.getRight() - 14).withWidth (6).reduced (0, 10);
    g.setColour (juce::Colour (0xff252e4d));
    g.fillRoundedRectangle (mr, 3);
    const float lvl = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (meter, -60.0f) + 60.0f) / 60.0f);
    g.setColour (meter > 0.98f ? juce::Colours::red : ui::gold);
    g.fillRoundedRectangle (mr.withTrimmedTop (mr.getHeight() * (1.0f - lvl)), 3);

    // global section panel
    auto gp = juce::Rectangle<float> (20, 510, (float) getWidth() - 40, 152);
    g.setColour (ui::panel);
    g.fillRoundedRectangle (gp, 10.0f);
    g.setColour (ui::textDim);
    g.setFont (font (11.0f, true));
    g.drawText ("STACK CHARACTER", gp.reduced (14, 8).removeFromTop (14), juce::Justification::left);
    g.drawText ("MIX", gp.reduced (14, 8).removeFromTop (14), juce::Justification::right);
    // divider before mix
    const float dx = (float) dry.getX() - 10.0f;
    g.setColour (ui::panelHi);
    g.fillRect (dx, gp.getY() + 28, 1.5f, gp.getHeight() - 40);

    // auto harmony panel
    auto ap = juce::Rectangle<float> (20, 674, (float) getWidth() - 40, 170);
    juce::ColourGradient pg (ui::panelHi, ap.getX(), ap.getY(), ui::panel, ap.getX(), ap.getBottom(), false);
    g.setGradientFill (pg);
    g.fillRoundedRectangle (ap, 10.0f);
    g.setColour (ui::gold.withAlpha (0.5f));
    g.drawRoundedRectangle (ap.reduced (0.5f), 10.0f, 1.2f);
    g.setColour (ui::gold);
    g.setFont (font (12.0f, true));
    g.drawText ("AUTO HARMONY  -  GENERATE AUDIO & DRAG TO YOUR DAW", ap.reduced (14, 8).removeFromTop (14), juce::Justification::left);
    g.setColour (proc.renderer.isBusy() ? ui::ice : ui::textDim);
    g.setFont (font (11.5f, true));
    g.drawText (statusText, ap.reduced (14, 8).removeFromTop (14), juce::Justification::right);

    // progress bar under the generate button
    if (proc.renderer.isBusy())
    {
        auto pb = generateBtn.getBounds().toFloat().withY ((float) generateBtn.getBottom() + 4).withHeight (4);
        g.setColour (juce::Colour (0xff252e4d));
        g.fillRoundedRectangle (pb, 2);
        g.setColour (ui::gold);
        g.fillRoundedRectangle (pb.withWidth (pb.getWidth() * juce::jlimit (0.0f, 1.0f, proc.renderer.progress.load())), 2);
    }
    if (chips.isEmpty())
    {
        g.setColour (ui::textDim.withAlpha (0.7f));
        g.setFont (font (12.0f));
        g.drawText ("Generated files appear here - drag them onto a track", chipView.getBounds(), juce::Justification::centredLeft);
    }
}

void HarmonyStackEditor::resized()
{
    const int rowY = 100;
    presetBox.setBounds (22, rowY, 300, 28);
    modeBox.setBounds (340, rowY, 120, 28);
    keyBox.setBounds (470, rowY, 70, 28);
    scaleBox.setBounds (550, rowY, 170, 28);

    keyboard.setBounds (20, 142, getWidth() - 40, 70);

    const int stripY = 224, stripH = 274, gap = 12;
    const int stripW = (getWidth() - 40 - gap * 3) / 4;
    for (int i = 0; i < 4; ++i) strips[i]->setBounds (20 + i * (stripW + gap), stripY, stripW, stripH);

    auto gpanel = juce::Rectangle<int> (20, 510, getWidth() - 40, 152).reduced (10, 0);
    gpanel.removeFromTop (28);
    gpanel.removeFromBottom (10);
    const int slot = gpanel.getWidth() / 12;
    auto take = [&] { return gpanel.removeFromLeft (slot); };
    doubleBtn.setBounds (take().withSizeKeepingCentre (slot - 12, 30));
    for (auto* k : { &width, &humanize, &formant, &breath, &glide, &vib, &vibRate, &gate })
        k->setBounds (take());
    for (auto* k : { &dry, &harmony, &output })
        k->setBounds (take());

    auto ap = juce::Rectangle<int> (20, 674, getWidth() - 40, 170).reduced (14, 0);
    ap.removeFromTop (30);
    auto rowA = ap.removeFromTop (60);
    stemZone.setBounds (rowA.removeFromLeft (340));
    rowA.removeFromLeft (12);
    captureBtn.setBounds (rowA.removeFromLeft (112).withSizeKeepingCentre (112, 40));
    rowA.removeFromLeft (10);
    autoKeyBtn.setBounds (rowA.removeFromLeft (98).withSizeKeepingCentre (98, 40));
    rowA.removeFromLeft (6);
    allStylesBtn.setBounds (rowA.removeFromLeft (108).withSizeKeepingCentre (108, 40));
    rowA.removeFromLeft (6);
    stemsBtn.setBounds (rowA.removeFromLeft (78).withSizeKeepingCentre (78, 40));
    rowA.removeFromLeft (12);
    generateBtn.setBounds (rowA.withSizeKeepingCentre (rowA.getWidth(), 46));

    ap.removeFromTop (14);
    auto rowB = ap.removeFromTop (56);
    folderBtn.setBounds (rowB.removeFromRight (86).withSizeKeepingCentre (86, 40));
    rowB.removeFromRight (8);
    previewBtn.setBounds (rowB.removeFromRight (100).withSizeKeepingCentre (100, 40));
    rowB.removeFromRight (12);
    chipView.setBounds (rowB);
    rebuildChips();
}

bool HarmonyStackEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}

void HarmonyStackEditor::filesDropped (const juce::StringArray& files, int, int)
{
    stemZone.dragHover = false;
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) { loadStemFile (juce::File (f)); break; }
}

void HarmonyStackEditor::loadStemFile (const juce::File& f)
{
    juce::String err;
    if (proc.loadStem (f, err)) statusText = "Stem loaded - pick a style and press GENERATE";
    else statusText = err;
    refreshStemZone();
}

void HarmonyStackEditor::refreshStemZone()
{
    seenStem = proc.stemSerial;
    if (proc.stem == nullptr) return;
    stemZone.overview = proc.stemOverview;
    stemZone.name = proc.stemName;
    const double secs = proc.stem->getNumSamples() / proc.stemSampleRate;
    stemZone.info = juce::String ((int) secs / 60) + ":" + juce::String ((int) secs % 60).paddedLeft ('0', 2)
                  + "  " + juce::String (proc.stemSampleRate / 1000.0, 1) + " kHz";
    stemZone.repaint();
}

void HarmonyStackEditor::rebuildChips()
{
    chips.clear();
    const auto& res = proc.lastResult;
    if (res.ok && ! res.files.empty())
    {
        juce::StringArray all;
        for (auto& f : res.files) if (f.file.existsAsFile()) all.add (f.file.getFullPathName());
        if (all.size() > 1)
            chips.add (new DragChip ("ALL FILES (" + juce::String (all.size()) + ")", all, ui::gold));
        int vi = 0;
        for (auto& f : res.files)
        {
            if (! f.file.existsAsFile()) continue;
            auto col = f.label.startsWith ("Voice") ? ui::voiceCol[juce::jlimit (0, 3, f.label.getTrailingIntValue() - 1)]
                     : (f.label == "Lead + Stack" ? ui::text : ui::ice);
            if (proc.allStyles || res.files.size() > 6) col = ui::voiceCol[vi++ % 4];
            chips.add (new DragChip (f.label, juce::StringArray (f.file.getFullPathName()), col));
        }
    }
    const int h = juce::jmax (40, chipView.getHeight() - 8);
    int x = 0;
    for (auto* c : chips)
    {
        const int w = 136;
        c->setBounds (x, 0, w, h);
        chipHolder.addAndMakeVisible (c);
        x += w + 8;
    }
    chipHolder.setSize (juce::jmax (x, 1), h);
    repaint();
}

void HarmonyStackEditor::timerCallback()
{
    const float in = proc.uiInputMidi.load();
    voiced = in > 0.0f;
    if (voiced)
    {
        const int n = (int) std::lround (in);
        const int cents = (int) std::lround ((in - (float) n) * 100.0f);
        pitchText = midiToName (in) + "  " + (cents >= 0 ? "+" : "") + juce::String (cents) + "c";
    }
    else pitchText = "--";

    keyboard.inputMidi = in;
    for (int v = 0; v < 4; ++v) keyboard.voiceMidi[v] = proc.uiVoiceMidi[(size_t) v].load();
    keyboard.repaint();

    const int mode = modeBox.getSelectedId() - 1;
    const int scale = scaleBox.getSelectedId() - 1;
    for (int v = 0; v < 4; ++v)
        strips[v]->refresh (mode, scale, proc.uiVoiceMidi[(size_t) v].load(), proc.uiHeldNotes.load());

    scaleBox.setEnabled (mode == 0);
    keyBox.setEnabled (mode == 0);
    width.setAlpha (doubleBtn.getToggleState() ? 1.0f : 0.4f);

    // ---- Auto Harmony panel state ----
    proc.pollBackground();
    if (seenStem != proc.stemSerial)
    {
        refreshStemZone();
        if (proc.stem != nullptr) statusText = "Captured " + proc.stemName + " - press GENERATE";
    }
    if (seenResult != proc.resultSerial)
    {
        seenResult = proc.resultSerial;
        const auto& r = proc.lastResult;
        statusText = r.message;
        if (r.ok && r.key >= 0)
            statusText = "Key " + juce::String (labels::noteName (r.key)) + (r.minor ? " minor" : " major") + " detected  -  " + r.message;
        rebuildChips();
    }
    const bool busy = proc.renderer.isBusy();
    if (busy) statusText = proc.renderer.getStatus() + "  " + juce::String ((int) (proc.renderer.progress.load() * 100)) + "%";
    generateBtn.setButtonText (busy ? "CANCEL" : "GENERATE");
    generateBtn.setEnabled (busy || proc.stem != nullptr);
    previewBtn.setEnabled (proc.lastResult.preview != nullptr);
    previewBtn.setButtonText (proc.isPreviewPlaying() ? "STOP" : "PREVIEW");
    folderBtn.setEnabled (true);
    if (proc.isCapturing())
        captureBtn.setButtonText ("REC " + juce::String ((int) proc.capturedSeconds() / 60) + ":" + juce::String ((int) proc.capturedSeconds() % 60).paddedLeft ('0', 2));
    else
        captureBtn.setButtonText (proc.isCaptureArmed() ? "ARMED - PLAY" : "CAPTURE");
    captureBtn.getProperties().set ("filled", proc.isCaptureArmed());
    captureBtn.repaint();
    repaint (20, 674, getWidth() - 40, 30);
    if (busy) repaint (generateBtn.getBounds().withHeight (generateBtn.getHeight() + 10));

    meter = proc.uiOutLevel.load();
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    repaint (getWidth() - 220, 10, 210, 70);
}
