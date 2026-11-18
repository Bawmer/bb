// Bub Leon Harmony Stack — offline (faster than real-time) harmony rendering.
// Output is latency-compensated: sample 0 of every stem lines up with sample 0 of the source,
// so the files can be dropped at the same position as the original vocal.
#pragma once
#include "../Presets.h"
#include <atomic>
#include <functional>

namespace hs
{

struct RenderOutput
{
    std::vector<float> stackL, stackR;                 // all voices (voice levels applied)
    std::vector<float> voiceL[4], voiceR[4];           // each voice incl. its double
    int64_t length = 0;
};

inline bool renderHarmony (const float* mono, int64_t numSamples, double sampleRate, const EngineSnapshot& snap,
                           RenderOutput& out, bool wantVoiceStems,
                           const std::function<bool (float)>& progress = {})   // return false to cancel
{
    HarmonyEngine eng;
    const int block = 1024;
    eng.prepare (sampleRate, block);
    eng.setGlobal (snap.g);
    eng.setGateDb (snap.gateDb);
    for (int i = 0; i < 4; ++i) eng.setVoice (i, snap.v[i]);

    const int64_t lat = eng.latencySamples();
    const int64_t tail = (int64_t) (0.6 * sampleRate);       // let humanize delays / doubles ring out
    const int64_t total = numSamples + lat + tail;
    out.length = numSamples + tail;

    out.stackL.assign ((size_t) out.length, 0.0f);
    out.stackR.assign ((size_t) out.length, 0.0f);
    for (int v = 0; v < 4; ++v)
    {
        out.voiceL[v].assign (wantVoiceStems ? (size_t) out.length : 0, 0.0f);
        out.voiceR[v].assign (wantVoiceStems ? (size_t) out.length : 0, 0.0f);
    }

    std::vector<float> in ((size_t) block), wl ((size_t) block), wr ((size_t) block);
    std::vector<float> vl[4], vr[4];
    float* vlp[4]; float* vrp[4];
    for (int v = 0; v < 4; ++v) { vl[v].resize ((size_t) block); vr[v].resize ((size_t) block); vlp[v] = vl[v].data(); vrp[v] = vr[v].data(); }

    for (int64_t pos = 0; pos < total; pos += block)
    {
        const int n = (int) std::min<int64_t> (block, total - pos);
        for (int i = 0; i < n; ++i)
        {
            const int64_t src = pos + i;
            in[(size_t) i] = src < numSamples ? mono[src] : 0.0f;
        }
        std::fill (wl.begin(), wl.begin() + n, 0.0f);
        std::fill (wr.begin(), wr.begin() + n, 0.0f);
        if (wantVoiceStems)
            for (int v = 0; v < 4; ++v) { std::fill (vl[v].begin(), vl[v].begin() + n, 0.0f); std::fill (vr[v].begin(), vr[v].begin() + n, 0.0f); }

        eng.process (in.data(), wl.data(), wr.data(), n, wantVoiceStems ? vlp : nullptr, wantVoiceStems ? vrp : nullptr);

        for (int i = 0; i < n; ++i)
        {
            const int64_t dst = pos + i - lat;      // remove engine latency
            if (dst < 0 || dst >= out.length) continue;
            out.stackL[(size_t) dst] = wl[(size_t) i];
            out.stackR[(size_t) dst] = wr[(size_t) i];
            if (wantVoiceStems)
                for (int v = 0; v < 4; ++v) { out.voiceL[v][(size_t) dst] = vl[v][(size_t) i]; out.voiceR[v][(size_t) dst] = vr[v][(size_t) i]; }
        }
        if (progress && (pos / block) % 64 == 0)
            if (! progress ((float) ((double) pos / (double) total))) return false;
    }
    return true;
}

} // namespace hs
