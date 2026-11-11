// FaceplateLookAndFeel.h — BUB LEON Oxford Ultra: photoreal faceplate skin.
// Knobs draw ONLY the live parts (groove, value arc, amber dot, cap tick) over the metal caps
// baked into Resources/faceplate.png. All geometry is in faceplate pixels (1295 x 832) and scaled.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace BubLeonGui
{
    namespace Colours
    {
        inline const juce::Colour arc       { 0xFF6FD6FF };
        inline const juce::Colour arcGlow   { 0xFF38BDF8 };
        inline const juce::Colour groove    { 0xFF1C232D };
        inline const juce::Colour amber     { 0xFFFFC94D };
        inline const juce::Colour valueText { 0xFFF3D892 };
        inline const juce::Colour hudText   { 0xFFCBD5E1 };
        inline const juce::Colour cyanText  { 0xFF67E8F9 };
        inline const juce::Colour lcdText   { 0xFF1E252C };
        inline const juce::Colour red       { 0xFFFF5A5A };
    }

    // Knob footprint: square of half-size 46 faceplate px around the cap centre.
    static constexpr float kKnobHalf = 46.0f;

    class FaceplateLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        FaceplateLookAndFeel()
        {
           #if JUCE_WINDOWS
            setDefaultSansSerifTypefaceName ("Segoe UI");
           #endif
            setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xF00B1220));
            setColour (juce::BubbleComponent::outlineColourId,    juce::Colour (0xFF38BDF8));
            setColour (juce::TooltipWindow::textColourId,         Colours::hudText);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider& s) override
        {
            const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            const float k = (b.getWidth() * 0.5f) / kKnobHalf;           // faceplate px -> screen px
            const float angle = startAngle + pos * (endAngle - startAngle);

            if (s.getProperties().getWithDefault ("bigKnob", false))
            {
                // Perspective cap without a ring: glowing dot on the cap face.
                const auto c = b.getCentre().translated ((float) s.getProperties()["faceDx"] * k,
                                                         (float) s.getProperties()["faceDy"] * k);
                const auto p = c.getPointOnCircumference (17.0f * k, angle);
                g.setColour (Colours::arcGlow.withAlpha (0.35f));
                g.fillEllipse (juce::Rectangle<float> (9.0f * k, 9.0f * k).withCentre (p));
                g.setColour (Colours::arc);
                g.fillEllipse (juce::Rectangle<float> (4.2f * k, 4.2f * k).withCentre (p));
                return;
            }

            const auto c = b.getCentre();
            const float r = 36.5f * k;

            // groove
            juce::Path groove;
            groove.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
            g.setColour (Colours::groove);
            g.strokePath (groove, juce::PathStrokeType (5.2f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // value arc + glow
            if (pos > 0.002f)
            {
                juce::Path arc;
                arc.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, angle, true);
                g.setColour (Colours::arcGlow.withAlpha (0.18f));
                g.strokePath (arc, juce::PathStrokeType (11.0f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (Colours::arcGlow.withAlpha (0.35f));
                g.strokePath (arc, juce::PathStrokeType (7.0f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (Colours::arc);
                g.strokePath (arc, juce::PathStrokeType (4.0f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            // amber position dot
            const auto dp = c.getPointOnCircumference (r, angle);
            g.setGradientFill (juce::ColourGradient (Colours::amber.withAlpha (0.55f), dp.x, dp.y,
                                                     Colours::amber.withAlpha (0.0f), dp.x + 9.0f * k, dp.y, true));
            g.fillEllipse (juce::Rectangle<float> (18.0f * k, 18.0f * k).withCentre (dp));
            g.setColour (Colours::amber);
            g.fillEllipse (juce::Rectangle<float> (7.4f * k, 7.4f * k).withCentre (dp));
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.fillEllipse (juce::Rectangle<float> (3.0f * k, 3.0f * k).withCentre (dp.translated (-0.6f * k, -0.6f * k)));

            // cap pointer tick
            const auto t0 = c.getPointOnCircumference (13.0f * k, angle);
            const auto t1 = c.getPointOnCircumference (21.5f * k, angle);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawLine ({ t0.translated (0.6f * k, 0.8f * k), t1.translated (0.6f * k, 0.8f * k) }, 2.4f * k);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawLine ({ t0, t1 }, 1.8f * k);
        }

        juce::Font getSliderPopupFont (juce::Slider&) override { return juce::FontOptions (14.0f, juce::Font::bold); }

        void drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body) override
        {
            g.setColour (findColour (juce::BubbleComponent::backgroundColourId));
            g.fillRoundedRectangle (body, 4.0f);
            g.setColour (findColour (juce::BubbleComponent::outlineColourId).withAlpha (0.7f));
            g.drawRoundedRectangle (body.reduced (0.5f), 4.0f, 1.0f);
        }

        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (juce::Colour (0xFF0D131C));
            g.setColour (juce::Colour (0xFF334155));
            g.drawRect (0, 0, w, h);
        }
    };

    // Pointer-free rotary knob that sits exactly over a baked cap.
    class FaceKnob final : public juce::Slider
    {
    public:
        FaceKnob()
        {
            setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            setMouseDragSensitivity (260);
            setVelocityModeParameters (0.6, 1, 0.0, true, juce::ModifierKeys::ctrlModifier);   // Ctrl = fine
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        bool hitTest (int x, int y) override
        {
            const auto c = getLocalBounds().toFloat().getCentre();
            return c.getDistanceFrom ({ (float) x, (float) y }) <= (float) getWidth() * 0.5f;
        }
    };

    // Text drawn into a baked black value box.
    class ValueBox final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<juce::String()> text;
        std::function<void()> onClick;

        void paint (juce::Graphics& g) override
        {
            if (! text) return;
            g.setColour (Colours::valueText);
            g.setFont (juce::FontOptions ((float) getHeight() * 0.72f, juce::Font::bold));
            g.drawFittedText (text(), getLocalBounds(), juce::Justification::centred, 1, 0.8f);
            if (onClick && isMouseOver())
            {
                g.setColour (Colours::arcGlow.withAlpha (0.5f));
                g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f, 1.0f);
            }
        }

        void mouseUp (const juce::MouseEvent& e) override { if (onClick && e.mouseWasClicked()) onClick(); }
        void mouseEnter (const juce::MouseEvent&) override { if (onClick) repaint(); }
        void mouseExit (const juce::MouseEvent&) override { if (onClick) repaint(); }
    };

    // Transparent toggle over a baked round button: lights an LED when on.
    class LedButton final : public juce::ToggleButton
    {
    public:
        LedButton() { setClickingTogglesState (true); setMouseCursor (juce::MouseCursor::PointingHandCursor); }

        float ledCentreYRatio = 0.36f;   // LED centre within component (faceplate layout)

        void paintButton (juce::Graphics& g, bool over, bool) override
        {
            const float s = (float) getWidth() / 84.0f;
            const auto c = juce::Point<float> ((float) getWidth() * 0.5f, (float) getHeight() * ledCentreYRatio);

            if (getToggleState())
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xAA7DD3FC), c.x, c.y,
                                                         juce::Colour (0x007DD3FC), c.x + 16.0f * s, c.y, true));
                g.fillEllipse (juce::Rectangle<float> (32.0f * s, 32.0f * s).withCentre (c));
                const auto sq = juce::Rectangle<float> (13.0f * s, 13.0f * s).withCentre (c);
                g.setColour (juce::Colour (0xFFBFEFFF));
                g.fillRoundedRectangle (sq, 2.5f * s);
                g.setColour (juce::Colours::white);
                g.fillRoundedRectangle (sq.reduced (2.6f * s), 1.5f * s);
            }
            else if (over)
            {
                g.setColour (juce::Colour (0x3338BDF8));
                g.drawEllipse (juce::Rectangle<float> (30.0f * s, 30.0f * s).withCentre (c), 1.2f * s);
            }
        }
    };
} // namespace BubLeonGui
