// Bub Leon Harmony Stack — harmony engine
// Pitch-synchronous overlap-add (TD-PSOLA) voice stacker with formant preservation.
// Plain C++17, no framework dependency: allocation happens only in prepare().
#pragma once
#include "PitchTracker.h"
#include <array>
#include <cstdint>
#include <random>

namespace hs
{

enum class Mode : int { Scale = 0, Chromatic = 1, Midi = 2 };

struct ScaleDef { const char* name; int n; int steps[12]; };

inline const ScaleDef* scaleTable()
{
    static const ScaleDef t[] = {
        { "Major",            7, { 0, 2, 4, 5, 7, 9, 11 } },
        { "Natural Minor",    7, { 0, 2, 3, 5, 7, 8, 10 } },
        { "Harmonic Minor",   7, { 0, 2, 3, 5, 7, 8, 11 } },
        { "Melodic Minor",    7, { 0, 2, 3, 5, 7, 9, 11 } },
        { "Dorian",           7, { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",         7, { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",           7, { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",       7, { 0, 2, 4, 5, 7, 9, 10 } },
        { "Major Pentatonic", 5, { 0, 2, 4, 7, 9 } },
        { "Minor Pentatonic", 5, { 0, 3, 5, 7, 10 } },
        { "Blues",            6, { 0, 3, 5, 6, 7, 10 } },
        { "Chromatic",       12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    };
    return t;
}
inline constexpr int kNumScales = 12;

class HarmonyEngine
{
public:
    static constexpr int kVoices  = 4;
    static constexpr int kEngines = kVoices * 2;   // main + double per voice

    struct VoiceParams
    {
        bool  enabled  = true;
        int   interval = 2;      // scale steps (Scale), semitones (Chromatic), ignored (MIDI)
        float gain     = 1.0f;   // linear
        float pan      = 0.0f;   // -1..1
        float detuneCents = 0.0f;
    };

    struct GlobalParams
    {
        Mode  mode = Mode::Scale;
        int   key = 0;            // 0 = C
        int   scale = 0;
        float humanize = 0.3f;    // 0..1
        bool  doubleOn = false;
        float doubleWidth = 0.7f; // 0..1
        float formantSemis = 0.0f;// -6..+6 (applied to harmonies only)
        bool  formantFollow = true; // keep formants of shifted voices natural
        float breath = 0.25f;     // level of unvoiced (consonant/breath) parts in harmonies
        float glideMs = 40.0f;
        float vibratoDepth = 0.0f;// cents-scale 0..1 (adds synthetic vibrato on harmonies)
        float vibratoRate = 5.5f; // Hz
    };

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        fs = sampleRate;
        tracker.prepare (fs);

        Tmax   = (double) tracker.maxPeriod();
        Tunv   = fs / 180.0;
        fsMin  = std::pow (2.0, -6.0 / 12.0);
        Twrite = (int) std::ceil (Tmax / fsMin * 1.06) + 4;   // grain reach incl. formant stretch
        latency = Twrite + (int) std::ceil (2.0 * Tmax) + 16;
        maxHumanDelay = (int) std::ceil (0.035 * fs);
        pCoef = (float) (1.0 - std::exp (-1.0 / (0.040 * fs)));
        cCoef = (float) (1.0 - std::exp (-1.0 / (0.120 * fs)));

        inRing.assign (kInSize, 0.0f);
        lpRing.assign (kInSize, 0.0f);
        marks.assign (kMarkSize, Mark {});
        for (auto& e : eng) e.acc.assign (kAccSize, 0.0f);
        reset();
    }

    void reset()
    {
        tracker.reset();
        std::fill (inRing.begin(), inRing.end(), 0.0f);
        std::fill (lpRing.begin(), lpRing.end(), 0.0f);
        t = 0;
        markCount = 0;
        nextCand = (int64_t) Tunv;
        uint32_t seed = 0x5EED1234u;
        for (int i = 0; i < kEngines; ++i)
        {
            auto& e = eng[(size_t) i];
            std::fill (e.acc.begin(), e.acc.end(), 0.0f);
            e.synPos = (double) Twrite;
            e.cursor = 0;
            e.semisSm = 0.0;
            e.gainSm = 0.0f;
            e.prevSnap = -10000;
            e.rng.seed (seed + (uint32_t) i * 7919u);
            e.driftTarget = e.drift = 0.0;
            e.driftTimer = 0.0;
            e.delayBase = uni (e);
            e.delaySm = 0.0;
            e.vibPhase = uni (e) * 6.283185307;
            e.panL = e.panR = 0.70710678f;
            e.lastTarget = -1.0f;
            e.pIn = e.pOut = 0.0f;
            e.corr = 1.0f;
        }
        held.fill (-1);
        heldCount = 0;
    }

    int  latencySamples() const noexcept { return latency; }
    void setGlobal (const GlobalParams& g) noexcept { gp = g; }
    void setVoice  (int i, const VoiceParams& v) noexcept { vp[(size_t) i] = v; }
    void setGateDb (float db) noexcept { tracker.gateDb = db; }
    int  heldNoteCount() const noexcept { return heldCount; }

    // ---- MIDI (used in MIDI mode: held notes become the harmony chord) ----
    void noteOn (int note) noexcept
    {
        for (int i = 0; i < heldCount; ++i) if (held[(size_t) i] == note) return;
        if (heldCount < (int) held.size()) held[(size_t) heldCount++] = note;
        std::sort (held.begin(), held.begin() + heldCount);
    }
    void noteOff (int note) noexcept
    {
        int w = 0;
        for (int i = 0; i < heldCount; ++i) if (held[(size_t) i] != note) held[(size_t) w++] = held[(size_t) i];
        heldCount = w;
    }
    void allNotesOff() noexcept { heldCount = 0; }

    // ---- analysis info for the GUI ----
    float detectedMidi() const noexcept { return tracker.voiced() ? tracker.midiNote() : -1.0f; }
    float voiceTargetMidi (int v) const noexcept { return eng[(size_t) v].lastTarget; }

    // mono in -> stereo harmony bus (adds into wetL / wetR; caller clears).
    // Optional per-voice stereo outputs (arrays of kVoices pointers) for stem rendering.
    void process (const float* in, float* wetL, float* wetR, int n,
                  float* const* voiceL = nullptr, float* const* voiceR = nullptr) noexcept
    {
        updatePans();
        const double humanDelayMax = gp.humanize * 0.018 * fs;
        for (int i = 0; i < kEngines; ++i)
        {
            auto& e = eng[(size_t) i];
            const bool isDouble = i >= kVoices;
            const double base = isDouble ? (0.012 * fs + e.delayBase * (0.006 * fs + humanDelayMax))
                                         : e.delayBase * humanDelayMax;
            e.delayTarget = std::min (base, (double) maxHumanDelay);
        }

        for (int s = 0; s < n; ++s)
        {
            const float x = in[s];
            inRing[(size_t) (t & kInMask)] = x;
            lpRing[(size_t) (t & kInMask)] = tracker.push (x);

            generateMarks (t + 1);

            float l = 0.0f, r = 0.0f;
            for (int i = 0; i < kEngines; ++i)
            {
                auto& e = eng[(size_t) i];
                e.delaySm += (e.delayTarget - e.delaySm) * 0.0002;
                while (e.synPos <= (double) (t + Twrite))
                    makeGrain (i);
                const size_t ai = (size_t) (t & kAccMask);
                float v = e.acc[ai];
                e.acc[ai] = 0.0f;
                // loudness match: harmony follows the energy of the (aligned) source
                const int64_t di = t - latency - (int64_t) e.delaySm;
                const float dIn = inRing[(size_t) (di & kInMask)];
                e.pIn  += (dIn * dIn * e.gainSm * e.gainSm - e.pIn) * pCoef;
                e.pOut += (v * v - e.pOut) * pCoef;
                if (e.pIn > 1.0e-10f && e.pOut > 1.0e-10f)
                {
                    const float target = std::clamp (std::sqrt (e.pIn / e.pOut), 0.5f, 2.0f);
                    e.corr += (target - e.corr) * cCoef;
                }
                v *= e.corr;
                l += v * e.panL;
                r += v * e.panR;
                if (voiceL != nullptr)
                {
                    voiceL[i % kVoices][s] += v * e.panL;
                    voiceR[i % kVoices][s] += v * e.panR;
                }
            }
            wetL[s] += l;
            wetR[s] += r;
            ++t;
        }
    }

private:
    struct Mark { int64_t pos = 0; float period = 240.0f; float midi = 60.0f; bool voiced = false; };

    struct Engine
    {
        std::vector<float> acc;
        double synPos = 0.0;
        int64_t cursor = 0;
        double semisSm = 0.0;
        float gainSm = 0.0f;
        int prevSnap = -10000;
        std::minstd_rand rng;
        double drift = 0.0, driftTarget = 0.0, driftTimer = 0.0;
        double delayBase = 0.0, delayTarget = 0.0, delaySm = 0.0;
        double vibPhase = 0.0;
        float panL = 0.707f, panR = 0.707f;
        float lastTarget = -1.0f;
        float pIn = 0.0f, pOut = 0.0f, corr = 1.0f;
    };

    static double uni (Engine& e) { return (double) (e.rng() - e.rng.min()) / (double) (e.rng.max() - e.rng.min()); }

    static constexpr int kInSize  = 1 << 17, kInMask  = kInSize - 1;
    static constexpr int kAccSize = 1 << 15, kAccMask = kAccSize - 1;
    static constexpr int kMarkSize = 1 << 13, kMarkMask = kMarkSize - 1;

    // -------- pitch marks (epochs) --------
    void generateMarks (int64_t inPos) noexcept
    {
        for (;;)
        {
            const bool voiced = tracker.voiced();
            const double T = voiced ? (double) tracker.period() : Tunv;
            const int64_t radius = voiced ? (int64_t) (T * 0.25) : 0;
            if (nextCand + radius >= inPos) break;

            int64_t m = nextCand;
            if (voiced && radius > 1)
            {
                float best = -1e30f;
                for (int64_t p = nextCand - radius; p <= nextCand + radius; ++p)
                {
                    const float v = lpRing[(size_t) (p & kInMask)];
                    if (v > best) { best = v; m = p; }
                }
            }
            if (markCount > 0)
            {
                const int64_t prev = marks[(size_t) ((markCount - 1) & kMarkMask)].pos;
                if (m <= prev + (int64_t) (T * 0.5)) m = prev + (int64_t) std::max (1.0, T * 0.5);
            }
            Mark mk;
            mk.pos = m;
            mk.period = (float) T;
            mk.voiced = voiced;
            mk.midi = voiced ? tracker.midiNote() : -1.0f;
            marks[(size_t) (markCount & kMarkMask)] = mk;
            ++markCount;
            nextCand = m + (int64_t) std::llround (T);
        }
    }

    // -------- harmony interval logic --------
    double targetSemis (int engIdx, int voiceIdx, float inMidi, bool& active) noexcept
    {
        auto& e = eng[(size_t) engIdx];
        const auto& v = vp[(size_t) voiceIdx];
        active = v.enabled;

        if (gp.mode == Mode::Chromatic)
        {
            e.lastTarget = inMidi + (float) v.interval;
            return (double) v.interval;
        }
        if (gp.mode == Mode::Midi)
        {
            if (voiceIdx >= heldCount) { active = false; return e.semisSm; }
            const int note = held[(size_t) voiceIdx];
            e.lastTarget = (float) note;
            // fold far-away notes to the nearest octave of the sung pitch (keeps formant shift sane)
            double d = (double) note - inMidi;
            while (d > 19.0) d -= 12.0;
            while (d < -24.0) d += 12.0;
            return d;
        }

        // Scale mode: diatonic interval, follows the singer's own expression
        const ScaleDef& sc = scaleTable()[std::clamp (gp.scale, 0, kNumScales - 1)];
        const double rel = (double) inMidi - (double) gp.key;
        const int N = sc.n;
        // nearest scale degree (global index)
        int bestIdx = 0; double bestDist = 1e9;
        const int oct = (int) std::floor (rel / 12.0);
        for (int o = oct - 1; o <= oct + 1; ++o)
            for (int d = 0; d < N; ++d)
            {
                const double note = o * 12 + sc.steps[d];
                const double dist = std::abs (note - rel);
                if (dist < bestDist) { bestDist = dist; bestIdx = o * N + d; }
            }
        // hysteresis against flicker between neighbouring degrees
        if (e.prevSnap > -10000)
        {
            const double prevNote = degreeToNote (sc, e.prevSnap);
            if (std::abs (prevNote - rel) < bestDist + 0.35) bestIdx = e.prevSnap;
        }
        e.prevSnap = bestIdx;
        const double snapped = degreeToNote (sc, bestIdx);
        const double target  = degreeToNote (sc, bestIdx + v.interval);
        e.lastTarget = (float) (target + gp.key);
        return target - snapped;
    }

    static double degreeToNote (const ScaleDef& sc, int idx) noexcept
    {
        const int N = sc.n;
        int o = idx >= 0 ? idx / N : -((-idx + N - 1) / N);
        int d = idx - o * N;
        return (double) (o * 12 + sc.steps[d]);
    }

    // -------- grain synthesis --------
    void makeGrain (int engIdx) noexcept
    {
        auto& e = eng[(size_t) engIdx];
        const int voiceIdx = engIdx % kVoices;
        const bool isDouble = engIdx >= kVoices;

        if (markCount == 0) { e.synPos += Tunv; return; }

        const double ti = e.synPos - (double) latency - e.delaySm;
        int64_t c = std::max (e.cursor, markCount - (int64_t) kMarkSize + 1);
        c = std::min (c, markCount - 1);
        while (c + 1 < markCount
               && std::abs ((double) marks[(size_t) ((c + 1) & kMarkMask)].pos - ti)
                  <= std::abs ((double) marks[(size_t) (c & kMarkMask)].pos - ti))
            ++c;
        e.cursor = c;
        const Mark& m = marks[(size_t) (c & kMarkMask)];

        const double Ta = m.period;
        bool active = vp[(size_t) voiceIdx].enabled && (! isDouble || gp.doubleOn);
        double ratio = 1.0;
        const double hopSec = Ta / fs;

        if (m.voiced)
        {
            bool voiceActive = true;
            double semis = targetSemis (engIdx, voiceIdx, m.midi, voiceActive);
            active = active && voiceActive;

            // humanize: slow random pitch drift
            e.driftTimer -= hopSec;
            if (e.driftTimer <= 0.0)
            {
                e.driftTimer = 0.15 + 0.45 * uni (e);
                e.driftTarget = (uni (e) * 2.0 - 1.0);
            }
            e.drift += (e.driftTarget - e.drift) * (1.0 - std::exp (-hopSec / 0.25));
            double cents = vp[(size_t) voiceIdx].detuneCents
                         + e.drift * gp.humanize * (isDouble ? 14.0 : 9.0)
                         + (isDouble ? (engIdx % 2 == 0 ? 7.0 : -7.0) * (0.4 + gp.humanize) : 0.0);
            e.vibPhase += 6.283185307 * gp.vibratoRate * hopSec;
            if (e.vibPhase > 6.283185307) e.vibPhase -= 6.283185307;
            cents += std::sin (e.vibPhase) * gp.vibratoDepth * 35.0;
            semis += cents / 100.0;

            const double glide = std::max (0.001, gp.glideMs * 0.001);
            if (e.gainSm < 0.01f) e.semisSm = semis;   // fresh entry: no glide from stale pitch
            e.semisSm += (semis - e.semisSm) * (1.0 - std::exp (-hopSec / glide));
            ratio = std::pow (2.0, e.semisSm / 12.0);
        }
        else
        {
            e.prevSnap = -10000;
        }

        // gain smoothing (per grain, crossfaded naturally by the overlap-add)
        const float gTarget = active ? (m.voiced ? 1.0f : gp.breath) * vp[(size_t) voiceIdx].gain
                                         * (isDouble ? 0.7079f : 1.0f)
                                     : 0.0f;
        e.gainSm += (gTarget - e.gainSm) * (float) (1.0 - std::exp (-hopSec / 0.012));

        // formant handling: PSOLA preserves formants (grain read at 1:1); optional shift
        double fsr = std::pow (2.0, (double) gp.formantSemis / 12.0);
        if (! gp.formantFollow) fsr *= std::sqrt (ratio);   // partial "chipmunk" character
        fsr = std::clamp (fsr, fsMin, 1.0 / fsMin);

        if (e.gainSm > 1.0e-5f)
        {
            const double Tg = Ta / fsr;
            const int half = (int) std::ceil (Tg);
            const double sum = ratio / fsr;                         // mean window overlap
            const float norm = (float) (sum > 1.0 ? 1.0 / sum : 1.0) * e.gainSm;
            const int64_t centre = (int64_t) std::llround (e.synPos);
            const double invTg = 3.14159265358979323846 / Tg;
            for (int k = -half + 1; k < half; ++k)
            {
                const double w = 0.5 * (1.0 + std::cos (k * invTg));
                if (w <= 0.0) continue;
                const double rp = (double) m.pos + k * fsr;
                const double fl = std::floor (rp);
                const int64_t ip = (int64_t) fl;
                const float fr = (float) (rp - fl);
                const float a = inRing[(size_t) (ip & kInMask)];
                const float b = inRing[(size_t) ((ip + 1) & kInMask)];
                e.acc[(size_t) ((centre + k) & kAccMask)] += norm * (float) w * (a + fr * (b - a));
            }
        }

        e.synPos += Ta / ratio;
    }

    void updatePans() noexcept
    {
        for (int i = 0; i < kEngines; ++i)
        {
            const int v = i % kVoices;
            const bool isDouble = i >= kVoices;
            float p = vp[(size_t) v].pan;
            if (gp.doubleOn)
                p = std::clamp (p + (isDouble ? 1.0f : -1.0f) * 0.6f * gp.doubleWidth, -1.0f, 1.0f);
            const float a = (p + 1.0f) * 0.25f * 3.14159265f;
            eng[(size_t) i].panL = std::cos (a);
            eng[(size_t) i].panR = std::sin (a);
        }
    }

    double fs = 48000.0, Tmax = 686.0, Tunv = 266.0, fsMin = 0.707;
    int Twrite = 1000, latency = 2400, maxHumanDelay = 1700;
    float pCoef = 0.0005f, cCoef = 0.0002f;
    PitchTracker tracker;
    std::vector<float> inRing, lpRing;
    std::vector<Mark> marks;
    int64_t markCount = 0, nextCand = 0, t = 0;
    std::array<Engine, kEngines> eng;
    std::array<VoiceParams, kVoices> vp;
    GlobalParams gp;
    std::array<int, 16> held {};
    int heldCount = 0;
};

} // namespace hs
