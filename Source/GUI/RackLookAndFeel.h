#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Engine/ModuleBase.h"

namespace bl
{
struct PlateColours
{
    juce::Colour panel, panelDark, text, knob, knobEdge, pointer, label;
};

inline PlateColours plateColours (Faceplate f)
{
    using C = juce::Colour;
    switch (f)
    {
        case Faceplate::Silver:   return { C (0xffc3c6ca), C (0xff8d9196), C (0xff1e1e20), C (0xff2a2a2c), C (0xffd8d8d8), C (0xfff2f2f2), C (0xff2a2a2a) };
        case Faceplate::Cream:    return { C (0xffe6dac0), C (0xffb9ab8a), C (0xff2a241a), C (0xff1d1d1d), C (0xff6b6253), C (0xfff4efe2), C (0xff3a3226) };
        case Faceplate::Blue:     return { C (0xff2f5f8e), C (0xff1d3c5c), C (0xfff2e9cf), C (0xffe9e1cb), C (0xff9c9682), C (0xff202020), C (0xffeee4c6) };
        case Faceplate::Graphite: return { C (0xff383b41), C (0xff222428), C (0xffe6e6e6), C (0xff1c1d20), C (0xff5d6168), C (0xffffffff), C (0xffc9ccd2) };
        case Faceplate::Bronze:   return { C (0xff5b4129), C (0xff3a2919), C (0xfff0d39a), C (0xffc9a35e), C (0xff7a5c32), C (0xff2a1d10), C (0xffe7c88a) };
        case Faceplate::Teal:     return { C (0xff21535a), C (0xff133338), C (0xffe8f6f6), C (0xff16191a), C (0xff4f7d82), C (0xffffffff), C (0xffcfe9ea) };
        case Faceplate::Red:      return { C (0xff7c2222), C (0xff4e1414), C (0xfffbeeee), C (0xff171717), C (0xff5a2a2a), C (0xffffffff), C (0xfff3dada) };
        case Faceplate::Black:
        default:                  return { C (0xff1d1e21), C (0xff111214), C (0xfff0f0f0), C (0xff2b2c30), C (0xffa9acb2), C (0xffffffff), C (0xffd2d4d8) };
    }
}

class RackLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RackLookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff17181b));
        setColour (juce::ComboBox::textColourId, juce::Colour (0xffe8e8e8));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3c42));
        setColour (juce::ComboBox::arrowColourId, juce::Colour (0xffe0a040));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1c20));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffe0a040));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff24262b));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe0a040));
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd8d8d8));
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xf0101114));
        setColour (juce::BubbleComponent::outlineColourId, juce::Colour (0xffe0a040));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0101114));
        setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
        setColour (juce::Label::textColourId, juce::Colour (0xffe0e0e0));
    }

    juce::Font getComboBoxFont (juce::ComboBox& b) override
    {
        const float h = juce::jmin (13.0f, b.getHeight() * 0.62f);
        return juce::Font (juce::FontOptions (b.getWidth() < 80 ? juce::jmin (h, 11.0f) : h));
    }

    static int comboArrowWidth (const juce::ComboBox& b) { return b.getWidth() < 80 ? 12 : 22; }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId).withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.5f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        const float aw = (float) comboArrowWidth (box);
        auto a = juce::Rectangle<float> ((float) w - aw, 0, aw - 3.0f, (float) h).withSizeKeepingCentre (juce::jmin (8.0f, aw - 4.0f), 4.5f);
        juce::Path p;
        p.startNewSubPath (a.getX(), a.getY()); p.lineTo (a.getCentreX(), a.getBottom()); p.lineTo (a.getRight(), a.getY());
        g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 0.9f : 0.25f));
        g.strokePath (p, juce::PathStrokeType (1.6f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (1, 1, box.getWidth() - comboArrowWidth (box) - 1, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setMinimumHorizontalScale (0.75f);
        label.setBorderSize (juce::BorderSize<int> (0, box.getWidth() < 80 ? 3 : 6, 0, 0));
    }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions (juce::jmin (13.0f, h * 0.6f), juce::Font::bold)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (14.0f)); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider& s) override
    {
        const auto plate = (Faceplate) (int) s.getProperties().getWithDefault ("plate", 0);
        const auto accent = juce::Colour ((uint32_t) (juce::int64) s.getProperties().getWithDefault ("accent", (juce::int64) 0xffe0a040));
        const auto pc = plateColours (plate);
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float size = juce::jmin (b.getWidth(), b.getHeight());
        b = b.withSizeKeepingCentre (size, size);
        const auto c = b.getCentre();
        const float rad = size * 0.5f, angle = startA + pos * (endA - startA);

        // scale ticks
        g.setColour (pc.label.withAlpha (0.55f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startA + (endA - startA) * i / 10.0f;
            const auto p1 = c.getPointOnCircumference (rad - 0.5f, a), p2 = c.getPointOnCircumference (rad - (i % 5 == 0 ? 4.5f : 3.0f), a);
            g.drawLine ({ p1, p2 }, i % 5 == 0 ? 1.4f : 0.9f);
        }

        // value arc
        const float arcR = rad - 6.0f;
        juce::Path track; track.addCentredArc (c.x, c.y, arcR, arcR, 0, startA, endA, true);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (track, juce::PathStrokeType (2.2f));
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const float from = bipolar ? (startA + endA) * 0.5f : startA;
        juce::Path arc; arc.addCentredArc (c.x, c.y, arcR, arcR, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (accent.withAlpha (s.isEnabled() ? 0.95f : 0.3f));
        g.strokePath (arc, juce::PathStrokeType (2.2f));

        // knob body
        const float kr = rad - 9.0f;
        auto knob = juce::Rectangle<float> (kr * 2, kr * 2).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (knob.translated (0, 2.0f).expanded (1.0f));
        juce::ColourGradient grad (pc.knob.brighter (0.35f), knob.getX(), knob.getY(), pc.knob.darker (0.45f), knob.getRight(), knob.getBottom(), false);
        g.setGradientFill (grad);
        g.fillEllipse (knob);
        g.setColour (pc.knobEdge);
        g.drawEllipse (knob, 1.2f);
        auto cap = knob.reduced (kr * 0.28f);
        juce::ColourGradient capG (pc.knob.brighter (0.6f), cap.getX(), cap.getY(), pc.knob.darker (0.2f), cap.getRight(), cap.getBottom(), true);
        g.setGradientFill (capG);
        g.fillEllipse (cap);

        // pointer
        const auto p1 = c.getPointOnCircumference (kr * 0.25f, angle), p2 = c.getPointOnCircumference (kr * 0.92f, angle);
        g.setColour (pc.pointer);
        g.drawLine ({ p1, p2 }, juce::jmax (1.6f, size * 0.045f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto base = b.getToggleState() ? findColour (juce::TextButton::buttonOnColourId) : bg;
        if (down) base = base.darker (0.2f); else if (over) base = base.brighter (0.12f);
        g.setGradientFill (juce::ColourGradient (base.brighter (0.15f), r.getX(), r.getY(), base.darker (0.25f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }
};
} // namespace bl
