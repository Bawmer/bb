// Bub Leon Harmony Stack — automatic key detection from a vocal stem
// Pitch-class histogram (YIN, duration + level weighted) correlated with Krumhansl-Kessler profiles.
#pragma once
#include "PitchTracker.h"
#include <array>

namespace hs
{

struct KeyResult
{
    int key = 0;          // 0 = C
    bool minor = false;
    float confidence = 0; // correlation of the winner (0..1)
    float voicedSeconds = 0;
    std::array<float, 12> chroma {};
};

inline KeyResult detectKey (const float* mono, int64_t numSamples, double sampleRate)
{
    PitchTracker tr;
    tr.prepare (sampleRate);
    KeyResult res;
    const int hop = (int) (sampleRate * 0.01);
    float envAcc = 0.0f;
    int64_t voicedCount = 0;
    for (int64_t i = 0; i < numSamples; ++i)
    {
        tr.push (mono[i]);
        envAcc = std::max (envAcc, std::abs (mono[i]));
        if ((i % hop) == hop - 1)
        {
            if (tr.voiced())
            {
                const float m = tr.midiNote();
                const int pc = ((int) std::lround (m) % 12 + 12) % 12;
                // emphasise stable notes: weight by closeness to the semitone centre
                const float dev = std::abs (m - std::round (m));
                const float w = std::sqrt (envAcc) * (1.0f - std::min (1.0f, dev * 2.0f) * 0.7f);
                res.chroma[(size_t) pc] += w;
                ++voicedCount;
            }
            envAcc = 0.0f;
        }
    }
    res.voicedSeconds = (float) (voicedCount * hop / sampleRate);

    static const float maj[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float minr[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

    auto corr = [&] (const float* prof, int k)
    {
        float mx = 0, mp = 0;
        for (int i = 0; i < 12; ++i) { mx += res.chroma[(size_t) i]; mp += prof[i]; }
        mx /= 12; mp /= 12;
        float sxy = 0, sxx = 0, syy = 0;
        for (int i = 0; i < 12; ++i)
        {
            const float x = res.chroma[(size_t) ((i + k) % 12)] - mx, y = prof[i] - mp;
            sxy += x * y; sxx += x * x; syy += y * y;
        }
        return (sxx > 0 && syy > 0) ? sxy / std::sqrt (sxx * syy) : 0.0f;
    };

    float best = -2.0f;
    for (int k = 0; k < 12; ++k)
    {
        const float cM = corr (maj, k), cm = corr (minr, k);
        if (cM > best) { best = cM; res.key = k; res.minor = false; }
        if (cm > best) { best = cm; res.key = k; res.minor = true; }
    }
    res.confidence = std::max (0.0f, best);
    return res;
}

} // namespace hs
