#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "DSP/Common.h"

namespace bl
{
/** Power LED style toggle: lit when the parameter is OFF (i.e. module active) if invertLit. */
class LedButton final : public juce::ToggleButton
{
public:
    LedButton (juce::Colour on, bool invert) : lit (on), invertLit (invert) {}
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        auto led = r.withSizeKeepingCentre (d, d);
        const bool on = invertLit ? ! getToggleState() : getToggleState();
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (led.expanded (1.5f));
        if (on)
        {
            g.setColour (lit.withAlpha (0.35f)); g.fillEllipse (led.expanded (2.5f));
            g.setGradientFill (juce::ColourGradient (lit.brighter (0.8f), led.getCentreX(), led.getCentreY(), lit.darker (0.3f), led.getRight(), led.getBottom(), true));
        }
        else
            g.setColour (juce::Colour (0xff3a2a20).interpolatedWith (lit, 0.1f));
        g.fillEllipse (led);
        if (over) { g.setColour (juce::Colours::white.withAlpha (0.25f)); g.drawEllipse (led, 1.0f); }
    }
private:
    juce::Colour lit; bool invertLit;
};

/** Vertical or horizontal level meter with peak hold, dB scale -60..+6. */
class LevelBar final : public juce::Component
{
public:
    explicit LevelBar (bool vertical = true, bool grMode = false) : vert (vertical), gr (grMode) {}

    void setLevel (float linearPeak, float linearRms = -1.0f)
    {
        const float pdb = (float) gainToDb (linearPeak);
        peakDb = pdb > peakDb ? pdb : peakDb - 1.2f;
        rmsDb = linearRms < 0 ? -100.0f : (float) gainToDb (linearRms);
        if (pdb > holdDb || --holdTicks <= 0) { holdDb = pdb; holdTicks = 45; }
        repaint();
    }
    void setGr (float db) { const float v = juce::jmax (0.0f, db); grDb = v > grDb ? v : grDb * 0.85f + v * 0.15f; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0b0c0e)); g.fillRoundedRectangle (r, 2.0f);
        auto in = r.reduced (1.5f);
        if (gr)
        {
            const float f = juce::jlimit (0.0f, 1.0f, grDb / 20.0f);
            g.setColour (juce::Colour (0xffe0a040));
            if (vert) g.fillRect (in.withHeight (in.getHeight() * f));
            else      g.fillRect (in.withWidth (in.getWidth() * f).withX (in.getRight() - in.getWidth() * f));
            return;
        }
        auto norm = [] (float db) { return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f); };
        auto fill = [&] (float db, juce::Colour col, float shrink)
        {
            const float f = norm (db);
            auto b = vert ? in.withTop (in.getBottom() - in.getHeight() * f) : in.withWidth (in.getWidth() * f);
            if (shrink > 0) b = vert ? b.reduced (in.getWidth() * shrink, 0) : b.reduced (0, in.getHeight() * shrink);
            g.setColour (col); g.fillRect (b);
        };
        juce::ColourGradient grad = vert
            ? juce::ColourGradient (juce::Colour (0xff35d07f), in.getX(), in.getBottom(), juce::Colour (0xffff4a3d), in.getX(), in.getY(), false)
            : juce::ColourGradient (juce::Colour (0xff35d07f), in.getX(), in.getY(), juce::Colour (0xffff4a3d), in.getRight(), in.getY(), false);
        grad.addColour (norm (-12.0f), juce::Colour (0xff9be05a));
        grad.addColour (norm (-3.0f), juce::Colour (0xfff1c232));
        {
            const float f = norm (peakDb);
            auto b = vert ? in.withTop (in.getBottom() - in.getHeight() * f) : in.withWidth (in.getWidth() * f);
            g.setGradientFill (grad); g.fillRect (b);
        }
        if (rmsDb > -99.0f) fill (rmsDb, juce::Colours::white.withAlpha (0.25f), 0.3f);
        const float hf = norm (holdDb);
        g.setColour (holdDb > -0.1f ? juce::Colour (0xffff4a3d) : juce::Colours::white.withAlpha (0.8f));
        if (vert) g.fillRect (in.getX(), in.getBottom() - in.getHeight() * hf, in.getWidth(), 1.5f);
        else      g.fillRect (in.getX() + in.getWidth() * hf - 1.5f, in.getY(), 1.5f, in.getHeight());
    }

private:
    bool vert, gr;
    float peakDb = -100.0f, rmsDb = -100.0f, holdDb = -100.0f, grDb = 0.0f; int holdTicks = 0;
};
} // namespace bl
