#pragma once
// Brushed-metal 19" rack skin: embedded textures, colours, fonts and drawing helpers
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"
#include "Engine/ModuleBase.h"

namespace bl::skin
{
// base design size (pixels of the reference artwork)
constexpr int kW = 2032, kH = 1120;

inline const juce::Colour amber     { 0xffffa634 };
inline const juce::Colour amberDim  { 0xff4a2a08 };
inline const juce::Colour lcdInk    { 0xff2a1704 };
inline const juce::Colour engrave   { 0xff3a3a3c };
inline const juce::Colour labelLt   { 0xfff4f2ee };
inline const juce::Colour labelDk   { 0xff2b241b };

inline juce::Image image (const char* data, int size) { return juce::ImageCache::getFromMemory (data, size); }
inline juce::Image background() { return image (BinaryData::bg_jpg, BinaryData::bg_jpgSize); }
inline juce::Image emptyPlate() { return image (BinaryData::plate_empty_jpg, BinaryData::plate_empty_jpgSize); }

// photographic sprites cut from the reference artwork
inline juce::Image knobChrome() { return image (BinaryData::knob_chrome_png, BinaryData::knob_chrome_pngSize); }
inline juce::Image knobGold()   { return image (BinaryData::knob_gold_png,   BinaryData::knob_gold_pngSize); }
inline juce::Image knobTrim()   { return image (BinaryData::knob_trim_png,   BinaryData::knob_trim_pngSize); }
inline juce::Image knobTop()    { return image (BinaryData::knob_top_png,    BinaryData::knob_top_pngSize); }
inline juce::Image vuUnit()     { return image (BinaryData::vu_unit_png,     BinaryData::vu_unit_pngSize); }
inline juce::Image toggleUp()   { return image (BinaryData::toggle_up_png,   BinaryData::toggle_up_pngSize); }

/** Thin orange-tan meter needle with a soft shadow. */
inline void drawNeedle (juce::Graphics& g, juce::Point<float> pivot, float r0, float r1, float angle, float thickness)
{
    auto pt = [&] (float rr) { return juce::Point<float> (pivot.x + std::sin (angle) * rr, pivot.y - std::cos (angle) * rr); };
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.drawLine (juce::Line<float> (pt (r0), pt (r1)).withShortenedEnd (0.5f).toFloat(), thickness * 1.6f);
    g.setColour (juce::Colour (0xffd28a3c));
    g.drawLine ({ pt (r0), pt (r1) }, thickness);
    g.setColour (juce::Colour (0x66ffe2b0));
    g.drawLine ({ pt (r0), pt (r1) }, thickness * 0.4f);
}

inline juce::Image plate (Faceplate f)
{
    switch (f)
    {
        case Faceplate::Silver:   return image (BinaryData::plate_silver_jpg,   BinaryData::plate_silver_jpgSize);
        case Faceplate::Cream:    return image (BinaryData::plate_cream_jpg,    BinaryData::plate_cream_jpgSize);
        case Faceplate::Blue:     return image (BinaryData::plate_blue_jpg,     BinaryData::plate_blue_jpgSize);
        case Faceplate::Graphite: return image (BinaryData::plate_graphite_jpg, BinaryData::plate_graphite_jpgSize);
        case Faceplate::Bronze:   return image (BinaryData::plate_bronze_jpg,   BinaryData::plate_bronze_jpgSize);
        case Faceplate::Teal:     return image (BinaryData::plate_teal_jpg,     BinaryData::plate_teal_jpgSize);
        case Faceplate::Red:      return image (BinaryData::plate_red_jpg,      BinaryData::plate_red_jpgSize);
        case Faceplate::Hammer:   return image (BinaryData::plate_hammer_jpg,   BinaryData::plate_hammer_jpgSize);
        case Faceplate::Black:
        default:                  return image (BinaryData::plate_black_jpg,    BinaryData::plate_black_jpgSize);
    }
}

inline bool isLight (Faceplate f) { return f == Faceplate::Cream; }

//==============================================================================
inline const juce::String& pickFont (std::initializer_list<const char*> prefs, const juce::String& fallback, juce::String& cache)
{
    if (cache.isEmpty())
    {
        const auto installed = juce::Font::findAllTypefaceNames();
        for (auto* n : prefs)
            if (installed.contains (n, true)) { cache = n; break; }
        if (cache.isEmpty()) cache = fallback;
    }
    return cache;
}

inline juce::Font sans (float size, bool bold = true)
{
    static juce::String cache;
    return juce::Font (juce::FontOptions (pickFont ({ "Arial", "Helvetica", "Liberation Sans", "DejaVu Sans" }, juce::Font::getDefaultSansSerifFontName(), cache),
                                          size, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::Font mono (float size, bool bold = false)
{
    static juce::String cache;
    return juce::Font (juce::FontOptions (pickFont ({ "Consolas", "Lucida Console", "DejaVu Sans Mono", "Liberation Mono", "Courier New" }, juce::Font::getDefaultMonospacedFontName(), cache),
                                          size, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
/** Chrome Phillips screw. */
inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float r)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (juce::Rectangle<float> (r * 2.4f, r * 2.4f).withCentre (c.translated (0.4f, 0.8f)));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9a9a9c), c.x - r * 0.7f, c.y - r * 0.7f, juce::Colour (0xff1c1c1e), c.x + r, c.y + r, true));
    g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
    g.setColour (juce::Colour (0xff0c0c0c));
    g.drawLine (c.x - r * 0.6f, c.y, c.x + r * 0.6f, c.y, r * 0.32f);
    g.drawLine (c.x, c.y - r * 0.6f, c.x, c.y + r * 0.6f, r * 0.32f);
}

/** Small round indicator LED. */
inline void drawLed (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col, float level)
{
    level = juce::jlimit (0.0f, 1.0f, level);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillEllipse (juce::Rectangle<float> (r * 2.5f, r * 2.5f).withCentre (c));
    if (level > 0.02f)
    {
        juce::ColourGradient halo (col.withAlpha (0.55f * level), c.x, c.y, col.withAlpha (0.0f), c.x + r * 3.2f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (juce::Rectangle<float> (r * 6.4f, r * 6.4f).withCentre (c));
    }
    const auto off = col.withMultipliedBrightness (0.22f).withMultipliedSaturation (0.7f);
    const auto on = off.interpolatedWith (col, level);
    g.setGradientFill (juce::ColourGradient (on.brighter (0.6f * level + 0.1f), c.x - r * 0.4f, c.y - r * 0.4f, on.darker (0.3f), c.x + r, c.y + r, true));
    g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
    if (level > 0.3f) { g.setColour (juce::Colours::white.withAlpha (0.6f * level)); g.fillEllipse (juce::Rectangle<float> (r * 0.7f, r * 0.55f).withCentre (c.translated (-r * 0.3f, -r * 0.35f))); }
}

/** Amber backlit LCD screen (preset display). */
inline void drawAmberLcd (juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::ColourGradient gr (juce::Colour (0xffffb45a), r.getCentreX(), r.getCentreY(), juce::Colour (0xffb35e14), r.getX(), r.getY(), true);
    g.setGradientFill (gr);
    g.fillRect (r);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.35f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX(), r.getY() + 14, false));
    g.fillRect (r.withHeight (14));
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRect (r, 1.5f);
}

/** Black display with amber segments (OS/CPU screen, LUFS readout). */
inline void drawBlackLcd (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d140b), r.getCentreX(), r.getY(), juce::Colour (0xff070503), r.getCentreX(), r.getBottom(), false));
    g.fillRect (r);
    g.setColour (juce::Colours::black);
    g.drawRect (r, 1.5f);
}

/** Seven-segment text (digits, '-', '.', spaces) with glow. */
inline void drawSevenSeg (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, juce::Colour on)
{
    static const uint8_t seg[10] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };
    int cells = 0;
    for (auto ch : text) if (ch != '.') ++cells;
    const float h = area.getHeight(), w = h * 0.5f, gap = h * 0.34f, t = h * 0.13f;
    const float total = cells * w + (cells - 1) * gap;
    float x = area.getCentreX() - total * 0.5f;
    const float y = area.getY();
    auto bar = [&] (juce::Path& p, float x0, float y0, float x1, float y1)
    {
        juce::Line<float> l (x0, y0, x1, y1);
        p.addLineSegment (l.withShortenedStart (t * 0.45f).withShortenedEnd (t * 0.45f), t);
    };
    juce::Path lit;
    for (int i = 0; i < text.length(); ++i)
    {
        const auto ch = text[i];
        if (ch == '.') { lit.addEllipse (x - gap * 0.5f - t * 0.5f, y + h - t, t, t); continue; }
        uint8_t m = 0;
        if (ch >= '0' && ch <= '9') m = seg[ch - '0'];
        else if (ch == '-') m = 0x40;
        else if (ch == 'i') m = 0x10;
        else if (ch == 'n') m = 0x54;
        else if (ch == 'f') m = 0x71;
        const float sk = h * 0.06f;   // italic skew
        const float x0 = x + sk, x1 = x + w + sk, xm0 = x + sk * 0.5f, xm1 = x + w + sk * 0.5f, xb0 = x, xb1 = x + w;
        const float ym = y + h * 0.5f, yb = y + h;
        if (m & 0x01) bar (lit, x0, y, x1, y);
        if (m & 0x02) bar (lit, x1, y, xm1, ym);
        if (m & 0x04) bar (lit, xm1, ym, xb1, yb);
        if (m & 0x08) bar (lit, xb0, yb, xb1, yb);
        if (m & 0x10) bar (lit, xm0, ym, xb0, yb);
        if (m & 0x20) bar (lit, x0, y, xm0, ym);
        if (m & 0x40) bar (lit, xm0, ym, xm1, ym);
        x += w + gap;
    }
    g.setColour (on.withAlpha (0.25f));
    g.strokePath (lit, juce::PathStrokeType (t * 0.9f));
    g.setColour (on);
    g.fillPath (lit);
}

/** Backlit analog VU face with needle. value = needle position 0..1. */
inline void drawVuMeter (juce::Graphics& g, juce::Rectangle<float> r, float value, bool small)
{
    // bezel + glass
    g.setColour (juce::Colour (0xff1a1a1a)); g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colour (0xff8a8a8c)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    auto face = r.reduced (small ? 3.0f : 5.0f);
    juce::ColourGradient gr (juce::Colour (0xffd6f0a4), face.getCentreX(), face.getBottom() - face.getHeight() * 0.15f,
                             juce::Colour (0xff4f7a2c), face.getX(), face.getY(), true);
    gr.addColour (0.55, juce::Colour (0xff9fcf6a));
    g.setGradientFill (gr);
    g.fillRoundedRectangle (face, 2.0f);

    const auto pivot = juce::Point<float> (face.getCentreX(), face.getBottom() + face.getHeight() * 0.55f);
    const float rad = face.getHeight() * 1.2f;
    const float a0 = -0.78f, a1 = 0.78f;   // radians from vertical
    auto pt = [&] (float a, float rr) { return juce::Point<float> (pivot.x + std::sin (a) * rr, pivot.y - std::cos (a) * rr); };

    // scale arc + ticks (red zone from 0 VU)
    juce::Path arc; arc.addCentredArc (pivot.x, pivot.y, rad * 0.88f, rad * 0.88f, 0, a0, a1, true);
    g.setColour (juce::Colour (0xff1c2412)); g.strokePath (arc, juce::PathStrokeType (small ? 0.9f : 1.3f));
    const float redStart = a0 + (a1 - a0) * 0.72f;
    juce::Path red; red.addCentredArc (pivot.x, pivot.y, rad * 0.86f, rad * 0.86f, 0, redStart, a1, true);
    g.setColour (juce::Colour (0xffd2281e)); g.strokePath (red, juce::PathStrokeType (small ? 2.0f : 3.4f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (a1 - a0) * i / 10.0f;
        g.setColour (a >= redStart ? juce::Colour (0xffb01c14) : juce::Colour (0xff1c2412));
        g.drawLine ({ pt (a, rad * 0.88f), pt (a, rad * (i % 2 == 0 ? 0.98f : 0.94f)) }, small ? 0.8f : 1.1f);
    }
    if (! small)
    {
        g.setColour (juce::Colour (0xff1c2412));
        g.setFont (sans (9.0f));
        const char* labels[] = { "20", "10", "7", "5", "3", "1", "0", "+1", "+3" };
        for (int i = 0; i < 9; ++i)
        {
            const float a = a0 + (a1 - a0) * i / 8.0f;
            g.drawText (labels[i], juce::Rectangle<float> (18, 10).withCentre (pt (a, rad * 1.07f)), juce::Justification::centred);
        }
        g.setFont (sans (10.0f));
        g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.45f).withHeight (14), juce::Justification::centred);
    }
    // needle + dome
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (face.toNearestInt());
        const float a = a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, value);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawLine ({ pt (a, rad * 0.25f).translated (1.5f, 1.5f), pt (a, rad * 0.97f).translated (1.5f, 1.5f) }, small ? 1.0f : 1.6f);
        g.setColour (juce::Colour (0xff141414));
        g.drawLine ({ pt (a, rad * 0.25f), pt (a, rad * 0.97f) }, small ? 1.0f : 1.6f);
        g.setColour (juce::Colour (0xff1a2010));
        g.fillEllipse (juce::Rectangle<float> (face.getWidth() * 0.34f, face.getHeight() * 0.5f).withCentre ({ face.getCentreX(), face.getBottom() + face.getHeight() * 0.1f }));
    }
    // glass highlight
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), face.getX(), face.getY(), juce::Colours::transparentWhite, face.getX(), face.getCentreY(), false));
    g.fillRoundedRectangle (face.withHeight (face.getHeight() * 0.5f), 2.0f);
}

/** dBFS -> VU needle position (-18 dBFS = 0 VU at 72 % of the scale). */
inline float vuPosition (float dbfs)
{
    const float vu = dbfs + 18.0f;                      // VU units
    return juce::jlimit (0.0f, 1.0f, vu < 0 ? 0.72f + vu / 20.0f * 0.72f : 0.72f + vu / 3.0f * 0.28f);
}
} // namespace bl::skin
