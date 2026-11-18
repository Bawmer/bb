#pragma once
#include <juce_core/juce_core.h>
#include "dsp/HarmonyEngine.h"

namespace labels
{
inline const char* noteName (int pc)
{
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((pc % 12) + 12) % 12];
}

inline juce::String midiToName (float midi)
{
    if (midi < 0.0f) return "--";
    const int n = (int) std::lround (midi);
    return juce::String (noteName (n)) + juce::String (n / 12 - 1);
}

inline juce::String intervalLabel (int mode, int scaleIdx, int value, int voiceIdx)
{
    if (mode == 2) return "MIDI note " + juce::String (voiceIdx + 1);
    if (mode == 1)
    {
        if (value == 0) return "Unison";
        return (value > 0 ? "+" : "") + juce::String (value) + " st";
    }
    const int n = hs::scaleTable()[juce::jlimit (0, hs::kNumScales - 1, scaleIdx)].n;
    if (value == 0) return "Unison";
    const int a = std::abs (value);
    const juce::String dir = value > 0 ? " up" : " down";
    if (n == 7)
    {
        static const char* names[] = { "Unison", "2nd", "3rd", "4th", "5th", "6th", "7th", "Octave",
                                       "9th", "10th", "11th", "12th", "13th" };
        return juce::String (names[juce::jmin (a, 12)]) + dir;
    }
    return juce::String (a) + (a == 1 ? " step" : " steps") + dir;
}
}
