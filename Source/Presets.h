// Bub Leon Harmony Stack — style presets (shared by live mode and the offline renderer)
#pragma once
#include "dsp/HarmonyEngine.h"

namespace hs
{

struct VoicePreset { bool on; int interval; float levelDb; float pan; float detune; };

struct StylePreset
{
    const char* name;
    int mode;   // 0 Scale (interval = scale steps), 1 Chromatic (semitones), 2 MIDI
    float humanize, dbl, width, formant, breath, glide, vibDepth, vibRate, dryDb, harmDb;  // % values 0..100
    VoicePreset v[4];
};

inline const StylePreset* stylePresets()
{
    static const StylePreset t[] = {
        { "Init - Third Above",    0, 25, 0,  60,  0.0f, 25, 40,  0, 5.5f, 0.0f, -3.0f,
          { { true, 2, 0, -0.25f, 0 }, { false, 4, 0, 0.25f, 0 }, { false, -2, 0, -0.6f, 0 }, { false, -7, 0, 0.6f, 0 } } },
        { "Pop Stack 3rd + 5th",   0, 30, 1,  65,  0.0f, 20, 35,  0, 5.5f, 0.0f, -4.0f,
          { { true, 2, 0, -0.35f, 0 }, { true, 4, -2, 0.35f, 0 }, { false, -2, 0, -0.6f, 0 }, { false, -7, 0, 0.6f, 0 } } },
        { "R&B Silk",              0, 35, 1,  70,  0.5f, 15, 90, 15, 5.2f, 0.0f, -4.0f,
          { { true, 2, -1, -0.4f, 0 }, { true, 4, -3, 0.4f, 0 }, { true, -2, -4, 0.0f, 0 }, { false, -7, 0, 0.6f, 0 } } },
        { "Gospel Choir",          0, 55, 1,  90,  0.0f, 15, 60, 10, 5.2f, 0.0f, -3.0f,
          { { true, 2, -1, -0.45f, 0 }, { true, 4, -2, 0.45f, 0 }, { true, -3, -3, -0.8f, 0 }, { true, -7, -5, 0.8f, 0 } } },
        { "Neo-Soul 7ths",         0, 40, 1,  75,  0.0f, 15, 70,  8, 5.0f, 0.0f, -5.0f,
          { { true, 2, -1, -0.5f, 0 }, { true, 4, -2, 0.5f, 0 }, { true, 6, -4, 0.0f, 0 }, { true, -3, -5, 0.3f, 0 } } },
        { "Kompa / Zouk Sweet",    0, 30, 1,  70,  0.0f, 20, 50, 10, 5.5f, 0.0f, -4.0f,
          { { true, 2, 0, -0.4f, 0 }, { true, -5, -2, 0.4f, 0 }, { false, 4, -6, 0.0f, 0 }, { false, -7, 0, 0.6f, 0 } } },
        { "Afro House Chant",      0, 45, 1, 100, -0.5f, 30, 25,  0, 5.5f, 0.0f, -3.0f,
          { { true, 0, -2, -0.8f, 8 }, { true, 0, -2, 0.8f, -8 }, { true, -7, -4, 0.0f, 0 }, { true, 4, -8, 0.3f, 0 } } },
        { "Trap Octave Stack",     1, 30, 1,  80,  0.0f, 35, 20,  0, 5.5f, 0.0f, -5.0f,
          { { true, -12, -2, -0.3f, 0 }, { true, 12, -9, 0.3f, 0 }, { false, 0, -6, -0.7f, 0 }, { false, 0, -6, 0.7f, 0 } } },
        { "Drill Dark",            1, 20, 1,  60, -2.0f, 30, 20,  0, 5.5f, 0.0f, -4.0f,
          { { true, -12, 0, 0.0f, 0 }, { true, -5, -6, 0.5f, 0 }, { false, 0, -6, -0.5f, 0 }, { false, 0, -6, 0.5f, 0 } } },
        { "EDM Anthem",            0, 30, 1, 100,  0.0f, 20, 30,  0, 5.5f, 0.0f, -4.0f,
          { { true, 4, -2, -0.5f, 0 }, { true, 7, -4, 0.5f, 0 }, { true, 9, -8, 0.0f, 0 }, { false, -7, 0, 0.6f, 0 } } },
        { "Ballad Soft Below",     0, 25, 0,  50,  0.0f, 20, 80,  5, 5.0f, 0.0f, -5.0f,
          { { true, -2, 0, -0.3f, 0 }, { true, -5, -5, 0.3f, 0 }, { false, 2, 0, 0.0f, 0 }, { false, -7, 0, 0.6f, 0 } } },
        { "Angelic Highs",         0, 35, 1,  85,  1.5f, 10, 70, 18, 5.0f, 0.0f, -5.0f,
          { { true, 4, -1, -0.5f, 0 }, { true, 7, -5, 0.5f, 0 }, { true, 9, -9, 0.0f, 0 }, { false, -7, 0, 0.6f, 0 } } },
        { "Deep Choir Bed",        0, 45, 1, 100, -1.5f,  5, 80,  8, 4.8f, -60.0f, 0.0f,
          { { true, -3, -2, -0.6f, 0 }, { true, -5, -2, 0.6f, 0 }, { true, -7, -1, -0.2f, 0 }, { true, -10, -4, 0.2f, 0 } } },
        { "Low Octave Thickener",  1, 15, 0,  50,  0.0f, 10, 25,  0, 5.5f, 0.0f, -8.0f,
          { { true, -12, 0, 0.0f, 0 }, { false, 7, 0, 0.3f, 0 }, { false, -5, 0, -0.6f, 0 }, { false, 12, 0, 0.6f, 0 } } },
        { "Power Fifths",          1, 25, 1,  70,  0.0f, 20, 30,  0, 5.5f, 0.0f, -5.0f,
          { { true, 7, 0, -0.4f, 0 }, { true, -5, -2, 0.4f, 0 }, { false, 12, 0, -0.6f, 0 }, { false, -12, 0, 0.6f, 0 } } },
        { "Wide Unison Doubler",   1, 40, 0,  60,  0.0f, 60, 30,  0, 5.5f, 0.0f, -4.0f,
          { { true, 0, 0, -0.85f, 9 }, { true, 0, 0, 0.85f, -9 }, { false, 0, -6, -0.4f, 14 }, { false, 0, -6, 0.4f, -14 } } },
        { "MIDI Chord Choir",      2, 40, 1,  80,  0.0f, 15, 30,  0, 5.5f, 0.0f, -3.0f,
          { { true, 0, 0, -0.6f, 0 }, { true, 0, -1, -0.2f, 0 }, { true, 0, -1, 0.2f, 0 }, { true, 0, -2, 0.6f, 0 } } },
    };
    return t;
}
inline constexpr int kNumStylePresets = 17;
inline constexpr int kMidiStyleIndex = 16;

// Snapshot of everything the engine needs (used by the renderer)
struct EngineSnapshot
{
    HarmonyEngine::GlobalParams g;
    HarmonyEngine::VoiceParams v[4];
    float gateDb = -50.0f;
    float dryGain = 1.0f, harmGain = 0.7f;
};

inline float presetDbToGain (float db) { return db <= -59.9f ? 0.0f : std::pow (10.0f, db / 20.0f); }

// Apply a style to a snapshot, keeping key/scale/gate from the session
inline void applyStyle (const StylePreset& p, EngineSnapshot& s)
{
    s.g.mode = (Mode) p.mode;
    s.g.humanize = p.humanize * 0.01f;
    s.g.doubleOn = p.dbl > 0.5f;
    s.g.doubleWidth = p.width * 0.01f;
    s.g.formantSemis = p.formant;
    s.g.breath = p.breath * 0.01f;
    s.g.glideMs = p.glide;
    s.g.vibratoDepth = p.vibDepth * 0.01f;
    s.g.vibratoRate = p.vibRate;
    s.dryGain = presetDbToGain (p.dryDb);
    s.harmGain = presetDbToGain (p.harmDb);
    for (int i = 0; i < 4; ++i)
    {
        s.v[i].enabled = p.v[i].on;
        s.v[i].interval = p.v[i].interval;
        s.v[i].gain = presetDbToGain (p.v[i].levelDb);
        s.v[i].pan = p.v[i].pan;
        s.v[i].detuneCents = p.v[i].detune;
    }
}

} // namespace hs
