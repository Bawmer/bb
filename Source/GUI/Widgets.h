#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "DSP/Common.h"
#include "GUI/Skin.h"

namespace bl
{
/** Round indicator LED button. With invertLit, it is lit while the parameter is OFF (module active). */
class LedButton final : public juce::ToggleButton
{
public:
    LedButton (juce::Colour on, bool invert) : lit (on), invertLit (invert) {}
    void setLedColour (juce::Colour c) { lit = c; repaint(); }
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat();
        const bool on = invertLit ? ! getToggleState() : getToggleState();
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.26f;
        skin::drawLed (g, r.getCentre(), rad, lit, on && isEnabled() ? 1.0f : 0.0f);
        if (over) { g.setColour (juce::Colours::white.withAlpha (0.25f)); g.drawEllipse (juce::Rectangle<float> (rad * 2.9f, rad * 2.9f).withCentre (r.getCentre()), 1.0f); }
    }
private:
    juce::Colour lit; bool invertLit;
};

/** Miniature bat-handle toggle switch (up = on). */
class ToggleSwitch final : public juce::ToggleButton
{
public:
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat();
        auto body = r.withSizeKeepingCentre (juce::jmin (16.0f, r.getWidth()), juce::jmin (22.0f, r.getHeight()));
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (body.translated (0.8f, 1.2f), 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2c2e), body.getX(), body.getY(), juce::Colour (0xff050505), body.getRight(), body.getBottom(), false));
        g.fillRoundedRectangle (body, 2.0f);
        g.setColour (juce::Colour (0xff7a7a7c)); g.drawRoundedRectangle (body.reduced (0.5f), 2.0f, 0.8f);
        const bool on = getToggleState();
        auto lever = body.withSizeKeepingCentre (body.getWidth() * 0.5f, body.getHeight() * 0.55f).translated (0, on ? -body.getHeight() * 0.18f : body.getHeight() * 0.18f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2f0), lever.getX(), lever.getY(), juce::Colour (0xff6a6a6c), lever.getRight(), lever.getBottom(), false));
        g.fillRoundedRectangle (lever, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (lever, 2.0f, 0.7f);
        if (over) { g.setColour (juce::Colours::white.withAlpha (0.2f)); g.drawRoundedRectangle (body, 2.0f, 1.0f); }
    }
};

/** Analog VU meter with simple needle ballistics (fed a peak level ~30x per second). */
class VuMeter final : public juce::Component
{
public:
    explicit VuMeter (bool isSmall) : small (isSmall) { setOpaque (false); }
    void pushLevel (float linearPeak)
    {
        const float target = skin::vuPosition ((float) gainToDb (linearPeak));
        pos += (target - pos) * (target > pos ? 0.45f : 0.12f);
        repaint();
    }
    void paint (juce::Graphics& g) override { skin::drawVuMeter (g, getLocalBounds().toFloat(), pos, small); }
private:
    bool small; float pos = 0.0f;
};

/** Row of amber LEDs showing gain reduction (1 / 3 / 6 / 10 / 15 dB). */
class GrLeds final : public juce::Component
{
public:
    void setGr (float db)
    {
        const float v = juce::jmax (0.0f, db);
        gr = v > gr ? v : gr * 0.8f + v * 0.2f;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        static const float steps[] = { 1.0f, 3.0f, 6.0f, 10.0f, 15.0f };
        const float w = (float) getWidth() / 5.0f;
        for (int i = 0; i < 5; ++i)
        {
            const float lvl = juce::jlimit (0.0f, 1.0f, (gr - steps[i] * 0.6f) / (steps[i] * 0.4f + 0.01f));
            skin::drawLed (g, { w * (i + 0.5f), getHeight() * 0.5f }, juce::jmin (w, (float) getHeight()) * 0.26f,
                           i == 4 ? juce::Colour (0xffff5a2a) : skin::amber, lvl);
        }
    }
private:
    float gr = 0.0f;
};
} // namespace bl
