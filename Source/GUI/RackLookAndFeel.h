#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Engine/ModuleBase.h"
#include "GUI/Skin.h"

namespace bl
{
enum class KnobStyle { Chrome = 0, Top = 1, Gold = 2, Trim = 3 };

class RackLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RackLookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0d0905));
        setColour (juce::ComboBox::textColourId, skin::amber);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff6a6a6c));
        setColour (juce::ComboBox::arrowColourId, skin::amber);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff141210));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe8e4dc));
        setColour (juce::PopupMenu::headerTextColourId, skin::amber);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, skin::amber);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a2c));
        setColour (juce::TextButton::buttonOnColourId, skin::amber);
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd8d8d8));
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xff3a1e02));
        setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xf0100c08));
        setColour (juce::BubbleComponent::outlineColourId, skin::amber);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0100c08));
        setColour (juce::TooltipWindow::textColourId, juce::Colour (0xffeee8dc));
        setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff6a6a6c));
    }

    //==========================================================================
    juce::Font getComboBoxFont (juce::ComboBox& b) override { return skin::mono (juce::jmin (b.getHeight() * 0.68f, 15.0f), true); }
    juce::Font getPopupMenuFont() override { return skin::sans (15.0f, false); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return skin::sans (juce::jmin (16.0f, h * 0.48f), true); }

    /** Small amber-on-black LCD selector for choice controls. */
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
        g.setColour (juce::Colour (0xff8c8c8e)); g.fillRoundedRectangle (r, 2.5f);
        g.setColour (juce::Colour (0xff2a2a2a)); g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, 1.0f);
        skin::drawBlackLcd (g, r.reduced (2.0f));
        auto a = juce::Rectangle<float> ((float) w - 11.0f, 0, 7.0f, (float) h).withSizeKeepingCentre (6.0f, 3.5f);
        juce::Path p; p.addTriangle (a.getX(), a.getY(), a.getRight(), a.getY(), a.getCentreX(), a.getBottom());
        g.setColour (skin::amber.withAlpha (box.isEnabled() ? 0.85f : 0.3f));
        g.fillPath (p);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (2, 1, box.getWidth() - 13, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setMinimumHorizontalScale (0.6f);
        label.setJustificationType (juce::Justification::centred);
        label.setBorderSize (juce::BorderSize<int> (0, 3, 0, 0));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
        g.setColour (juce::Colour (0xff5a5a5c));
        g.drawRect (0, 0, w, h, 1);
    }

    //==========================================================================
    /** Pill-shaped rack buttons; "on" glows amber. */
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const float rad = r.getHeight() * 0.5f;
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (0, 2.0f), rad);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a4c), r.getX(), r.getY(), juce::Colour (0xff101010), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        auto in = r.reduced (3.0f);
        const float ri = in.getHeight() * 0.5f;
        if (b.getToggleState())
        {
            g.setColour (skin::amber.withAlpha (0.35f)); g.fillRoundedRectangle (r.expanded (3.0f), rad + 3.0f);
            juce::ColourGradient gr (juce::Colour (0xffffd28a), in.getCentreX(), in.getY(), juce::Colour (0xffe07a10), in.getCentreX(), in.getBottom(), false);
            g.setGradientFill (gr);
        }
        else
        {
            auto base = juce::Colour (down ? 0xff3a3a3c : (over ? 0xff6a6a6c : 0xff58585a));
            g.setGradientFill (juce::ColourGradient (base.brighter (0.25f), in.getX(), in.getY(), base.darker (0.5f), in.getX(), in.getBottom(), false));
        }
        g.fillRoundedRectangle (in, ri);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawLine (in.getX() + ri, in.getY() + 1.5f, in.getRight() - ri, in.getY() + 1.5f, 1.0f);
    }

    //==========================================================================
    /** Photographic knob sprite (cut from the reference artwork) with a white tick ring and a live pointer. */
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider& s) override
    {
        const auto style = (KnobStyle) (int) s.getProperties().getWithDefault ("knob", 0);
        const bool light = (bool) s.getProperties().getWithDefault ("lightPlate", false);
        const bool ticks = (bool) s.getProperties().getWithDefault ("ticks", true);

        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float size = juce::jmin (b.getWidth(), b.getHeight());
        b = b.withSizeKeepingCentre (size, size);
        const auto c = b.getCentre();
        const float rad = size * 0.5f, angle = startA + pos * (endA - startA);
        const float kr = ticks ? rad * 0.70f : rad;

        if (ticks)
        {
            for (int i = 0; i <= 10; ++i)
            {
                const float a = startA + (endA - startA) * i / 10.0f;
                const auto p1 = c.getPointOnCircumference (rad - 0.5f, a), p2 = c.getPointOnCircumference (kr + rad * 0.14f, a);
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.drawLine ({ p1.translated (0.6f, 0.9f), p2.translated (0.6f, 0.9f) }, juce::jmax (1.2f, size * 0.022f));
                g.setColour (light ? juce::Colour (0xff3a3128) : juce::Colour (0xffe9e6df));
                g.drawLine ({ p1, p2 }, juce::jmax (1.2f, size * 0.022f));
            }
        }

        // drop shadow
        auto knob = juce::Rectangle<float> (kr * 2, kr * 2).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (knob.translated (kr * 0.06f, kr * 0.14f).expanded (kr * 0.07f));
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (knob.translated (kr * 0.10f, kr * 0.26f).expanded (kr * 0.12f));

        juce::Image img;
        switch (style)
        {
            case KnobStyle::Top:  img = skin::knobTop(); break;
            case KnobStyle::Gold: img = skin::knobGold(); break;
            case KnobStyle::Trim: img = skin::knobTrim(); break;
            case KnobStyle::Chrome:
            default:              img = skin::knobChrome(); break;
        }
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (1.0f);   // drawImage uses the current colour's alpha
        g.drawImage (img, knob, juce::RectanglePlacement::stretchToFit);

        // pointer on the cap
        const auto p1 = c.getPointOnCircumference (kr * 0.30f, angle), p2 = c.getPointOnCircumference (kr * 0.74f, angle);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawLine ({ p1.translated (0.5f, 0.7f), p2.translated (0.5f, 0.7f) }, juce::jmax (1.6f, kr * 0.09f));
        g.setColour (style == KnobStyle::Chrome ? juce::Colour (0xff1a1a1a) : juce::Colour (0xfffff4dc));
        g.drawLine ({ p1, p2 }, juce::jmax (1.3f, kr * 0.07f));
        if (! s.isEnabled()) { g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillEllipse (knob); }
    }
};
} // namespace bl
