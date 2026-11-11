// Presets.h — BUB LEON Oxford Ultra: factory presets.
// Exposed to the host as programs (Cubase preset list) and in the LCD menu.
// Index 0 is "Init" so a host that selects program 0 on load never changes a session.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "OxfordUltraLimiter.h"

namespace BubLeonPresets
{
    struct Value  { const char* id; float value; };
    struct Preset { const char* name; std::initializer_list<Value> values; };

    // Values are in parameter units (dB, ms, Hz, 0..1 ratios, choice index, bool 0/1).
    inline const std::array<Preset, 9>& all()
    {
        static const std::array<Preset, 9> presets {{
            { "Init", {
                { "inputGain", 0.0f }, { "threshold", -0.10f }, { "attack", 1.50f }, { "release", 120.0f }, { "autoRelease", 0.80f },
                { "enhance", 0.35f }, { "transientPunch", 0.20f }, { "crossoverHz", 110.0f }, { "lfHeadroom", 0.78f }, { "bandCoupling", 0.15f },
                { "msMode", 1 }, { "midSideLink", 0.35f }, { "sideLowMono", 0.65f }, { "enhanceSideBoost", 0.30f }, { "stereoWidth", 1.05f },
                { "ditherBits", 1 }, { "ditherShaper", 2 }, { "autoBlanking", 1 }, { "oversampling", 1 } } },

            // Modern, competitive but open master. Streaming-safe -1 dBTP ceiling, mostly-linked
            // detector to hold the stereo image, mono sub below 120 Hz, gentle density, no width tricks.
            { "Pro Engineer", {
                { "inputGain", 4.0f }, { "threshold", -1.00f }, { "attack", 2.00f }, { "release", 150.0f }, { "autoRelease", 0.85f },
                { "enhance", 0.20f }, { "transientPunch", 0.25f }, { "crossoverHz", 120.0f }, { "lfHeadroom", 0.85f }, { "bandCoupling", 0.20f },
                { "msMode", 1 }, { "midSideLink", 0.80f }, { "sideLowMono", 0.80f }, { "enhanceSideBoost", 0.15f }, { "stereoWidth", 1.00f },
                { "ditherBits", 1 }, { "ditherShaper", 0 }, { "autoBlanking", 1 }, { "oversampling", 1 } } },

            { "Streaming Transparent", {
                { "inputGain", 2.0f }, { "threshold", -1.00f }, { "attack", 3.00f }, { "release", 200.0f }, { "autoRelease", 0.90f },
                { "enhance", 0.10f }, { "transientPunch", 0.15f }, { "crossoverHz", 110.0f }, { "lfHeadroom", 0.90f }, { "bandCoupling", 0.15f },
                { "msMode", 1 }, { "midSideLink", 0.90f }, { "sideLowMono", 0.60f }, { "enhanceSideBoost", 0.00f }, { "stereoWidth", 1.00f },
                { "ditherBits", 1 }, { "ditherShaper", 0 }, { "autoBlanking", 1 }, { "oversampling", 1 } } },

            { "Club Loud", {
                { "inputGain", 8.0f }, { "threshold", -0.30f }, { "attack", 1.00f }, { "release", 80.0f }, { "autoRelease", 0.70f },
                { "enhance", 0.60f }, { "transientPunch", 0.35f }, { "crossoverHz", 100.0f }, { "lfHeadroom", 0.78f }, { "bandCoupling", 0.25f },
                { "msMode", 1 }, { "midSideLink", 0.70f }, { "sideLowMono", 0.90f }, { "enhanceSideBoost", 0.30f }, { "stereoWidth", 1.05f },
                { "ditherBits", 1 }, { "ditherShaper", 0 }, { "autoBlanking", 1 }, { "oversampling", 1 } } },

            { "Hip-Hop / 808", {
                { "inputGain", 6.0f }, { "threshold", -0.50f }, { "attack", 1.50f }, { "release", 120.0f }, { "autoRelease", 0.80f },
                { "enhance", 0.45f }, { "transientPunch", 0.40f }, { "crossoverHz", 90.0f }, { "lfHeadroom", 0.72f }, { "bandCoupling", 0.10f },
                { "msMode", 1 }, { "midSideLink", 0.75f }, { "sideLowMono", 1.00f }, { "enhanceSideBoost", 0.20f }, { "stereoWidth", 1.05f },
                { "ditherBits", 1 }, { "ditherShaper", 0 }, { "autoBlanking", 1 }, { "oversampling", 1 } } },

            { "Rock Wall", {
                { "inputGain", 6.0f }, { "threshold", -0.50f }, { "attack", 0.80f }, { "release", 60.0f }, { "autoRelease", 0.75f },
                { "enhance", 0.50f }, { "transientPunch", 0.20f }, { "crossoverHz", 110.0f }, { "lfHeadroom", 0.80f }, { "bandCoupling", 0.25f },
                { "msMode", 1 }, { "midSideLink", 0.80f }, { "sideLowMono", 0.70f }, { "enhanceSideBoost", 0.20f }, { "stereoWidth", 1.00f },
                { "ditherBits", 1 }, { "ditherShaper", 0 }, { "autoBlanking", 1 }, { "oversampling", 1 } } },

            { "Acoustic / Jazz", {
                { "inputGain", 1.5f }, { "threshold", -1.00f }, { "attack", 5.00f }, { "release", 300.0f }, { "autoRelease", 0.90f },
                { "enhance", 0.00f }, { "transientPunch", 0.00f }, { "crossoverHz", 80.0f }, { "lfHeadroom", 0.95f }, { "bandCoupling", 0.05f },
                { "msMode", 0 }, { "midSideLink", 1.00f }, { "sideLowMono", 0.30f }, { "enhanceSideBoost", 0.00f }, { "stereoWidth", 1.00f },
                { "ditherBits", 1 }, { "ditherShaper", 0 }, { "autoBlanking", 1 }, { "oversampling", 1 } } },

            { "Vocal Forward", {
                { "inputGain", 3.5f }, { "threshold", -1.00f }, { "attack", 2.50f }, { "release", 180.0f }, { "autoRelease", 0.85f },
                { "enhance", 0.25f }, { "transientPunch", 0.15f }, { "crossoverHz", 120.0f }, { "lfHeadroom", 0.80f }, { "bandCoupling", 0.30f },
                { "msMode", 1 }, { "midSideLink", 0.85f }, { "sideLowMono", 0.80f }, { "enhanceSideBoost", 0.00f }, { "stereoWidth", 0.95f },
                { "ditherBits", 1 }, { "ditherShaper", 0 }, { "autoBlanking", 1 }, { "oversampling", 1 } } },

            { "CD Master 16-Bit", {
                { "inputGain", 4.0f }, { "threshold", -0.30f }, { "attack", 2.00f }, { "release", 150.0f }, { "autoRelease", 0.85f },
                { "enhance", 0.25f }, { "transientPunch", 0.25f }, { "crossoverHz", 120.0f }, { "lfHeadroom", 0.85f }, { "bandCoupling", 0.20f },
                { "msMode", 1 }, { "midSideLink", 0.80f }, { "sideLowMono", 0.80f }, { "enhanceSideBoost", 0.15f }, { "stereoWidth", 1.00f },
                { "ditherBits", 2 }, { "ditherShaper", 2 }, { "autoBlanking", 1 }, { "oversampling", 1 } } },
        }};
        return presets;
    }

    inline int count() noexcept { return (int) all().size(); }

    // Message thread. Each parameter change is wrapped in a gesture so hosts record it cleanly.
    inline void apply (juce::AudioProcessorValueTreeState& apvts, int index)
    {
        if (index < 0 || index >= count()) return;
        for (const auto& v : all()[(size_t) index].values)
            if (auto* p = apvts.getParameter (v.id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (v.value));
                p->endChangeGesture();
            }
    }

    // True when every parameter still matches the preset (within one display step).
    inline bool matches (const juce::AudioProcessorValueTreeState& apvts, int index)
    {
        if (index < 0 || index >= count()) return false;
        for (const auto& v : all()[(size_t) index].values)
            if (auto* p = apvts.getParameter (v.id))
                if (std::abs (p->getValue() - p->convertTo0to1 (v.value)) > 0.002f)
                    return false;
        return true;
    }
}
