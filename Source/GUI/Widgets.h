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
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.15f;   // leave room for the halo
        skin::drawLed (g, r.getCentre(), rad, lit, on && isEnabled() ? 1.0f : 0.0f);
        if (over) { g.setColour (juce::Colours::white.withAlpha (0.25f)); g.drawEllipse (juce::Rectangle<float> (rad * 2.9f, rad * 2.9f).withCentre (r.getCentre()), 1.0f); }
    }
private:
    juce::Colour lit; bool invertLit;
};

/** Miniature paddle toggle (photo sprite from the artwork; up = on). */
class ToggleSwitch final : public juce::ToggleButton
{
public:
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto img = skin::toggleUp();
        auto r = getLocalBounds().toFloat();
        const float h = juce::jmin (r.getHeight(), r.getWidth() * 27.0f / 22.0f);
        auto dst = r.withSizeKeepingCentre (h * 22.0f / 27.0f, h);
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRect (dst.translated (1.0f, 1.5f));
        g.setOpacity (1.0f);
        if (getToggleState())
            g.drawImage (img, dst, juce::RectanglePlacement::stretchToFit);
        else
        {
            juce::Graphics::ScopedSaveState s (g);
            g.addTransform (juce::AffineTransform::verticalFlip (dst.getY() * 2.0f + dst.getHeight()));
            g.drawImage (img, dst, juce::RectanglePlacement::stretchToFit);
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.fillRect (dst);
        }
        if (over) { g.setColour (juce::Colours::white.withAlpha (0.18f)); g.drawRect (dst, 1.0f); }
    }
};

/** VU meter: the unit version draws the photo face + needle, the main version only the needle over the printed face. */
class VuMeter final : public juce::Component
{
public:
    explicit VuMeter (bool isUnit) : unit (isUnit) {}
    void pushLevel (float linearPeak)
    {
        const float target = skin::vuPosition ((float) gainToDb (linearPeak));
        pos += (target - pos) * (target > pos ? 0.45f : 0.12f);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (unit)
        {
            g.setOpacity (1.0f);
            g.drawImage (skin::vuUnit(), r, juce::RectanglePlacement::stretchToFit);
            const float sx = r.getWidth() / 134.0f, sy = r.getHeight() / 42.0f;
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (juce::Rectangle<float> (6 * sx, 4 * sy, 122 * sx, 30 * sy).toNearestInt());
            const float a = -0.86f + 1.72f * pos;
            skin::drawNeedle (g, { 67.0f * sx, 41.0f * sy }, 9.0f * sy, 36.0f * sy, a, 1.3f * sx);
        }
        else
        {
            // component bounds = printed face; pivot sits below the face like the artwork
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (getLocalBounds());
            const float a = -0.83f + 1.66f * pos;
            skin::drawNeedle (g, { r.getCentreX(), r.getBottom() + 22.0f }, 34.0f, 102.0f, a, 2.0f);
        }
    }
private:
    bool unit; float pos = 0.0f;
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
