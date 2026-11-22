#pragma once
// 4-Band EQ, Passive Tube EQ
#include "Engine/ModuleBase.h"

namespace bl
{
//==============================================================================
class FourBandEQ final : public ModuleBase
{
public:
    static const ModuleInfo& staticInfo()
    {
        static const ModuleInfo i = []
        {
            ModuleInfo m;
            m.id = "eq4"; m.name = "4-Band EQ"; m.shortName = "EQ4"; m.category = "EQ";
            m.faceplate = Faceplate::Graphite; m.accent = 0xff4fb3e8;
            m.params = {
                withOffMin (pLog ("High Pass", "HPF", 20.0, 500.0, 20.0, "Hz")),
                withOffMax (pLog ("Low Pass", "LPF", 3000.0, 22000.0, 22000.0, "Hz")),
                pLin ("Low Gain", "LF GAIN", -15.0, 15.0, 0.0, "dB"),
                pLog ("Low Freq", "LF FREQ", 30.0, 450.0, 100.0, "Hz"),
                pLin ("Low-Mid Gain", "LMF GAIN", -15.0, 15.0, 0.0, "dB"),
                pLog ("Low-Mid Freq", "LMF FREQ", 200.0, 2500.0, 600.0, "Hz"),
                pLog ("Low-Mid Q", "LMF Q", 0.4, 4.0, 0.9, "", 2),
                pLin ("High-Mid Gain", "HMF GAIN", -15.0, 15.0, 0.0, "dB"),
                pLog ("High-Mid Freq", "HMF FREQ", 600.0, 7000.0, 2500.0, "Hz"),
                pLog ("High-Mid Q", "HMF Q", 0.4, 4.0, 0.9, "", 2),
                pLin ("High Gain", "HF GAIN", -15.0, 15.0, 0.0, "dB"),
                pLog ("High Freq", "HF FREQ", 1500.0, 16000.0, 8000.0, "Hz"),
            };
            return m;
        }();
        return i;
    }
    const ModuleInfo& info() const override { return staticInfo(); }

    void prepare (double newFs, int, int) override
    {
        fs = newFs;
        glide = 1.0 - std::exp (-(double) kChunk / (0.025 * fs));   // 25 ms parameter glide
        reset();
    }
    void reset() override { for (auto& b : f) b.reset(); primed = false; }

    void setParams (const double* p) override
    {
        for (int k = 0; k < 12; ++k) target[k] = p[k];
        if (! primed) { for (int k = 0; k < 12; ++k) cur[k] = target[k]; recompute(); primed = true; }
    }

    void process (double* l, double* r, int n) override
    {
        double* ch[2] = { l, r };
        for (int start = 0; start < n; start += kChunk)
        {
            const int len = std::min (kChunk, n - start);
            if (step()) recompute();
            for (int b = 0; b < 6; ++b)
            {
                if (! active[b]) continue;
                for (int c = 0; c < 2; ++c)
                    for (int i = start; i < start + len; ++i) ch[c][i] = f[b].process (ch[c][i], c);
            }
        }
    }

private:
    static constexpr int kChunk = 32;
    static bool isLogParam (int k) { return k == 0 || k == 1 || k == 3 || k == 5 || k == 6 || k == 8 || k == 9 || k == 11; }

    /** Glide the current values toward the targets; returns true if anything moved. */
    bool step()
    {
        bool moved = false;
        for (int k = 0; k < 12; ++k)
        {
            if (cur[k] == target[k]) continue;
            moved = true;
            if (isLogParam (k)) cur[k] *= std::pow (target[k] / cur[k], glide);
            else                cur[k] += (target[k] - cur[k]) * glide;
            if (std::abs (cur[k] - target[k]) <= 1.0e-4 * (std::abs (target[k]) + 1.0)) cur[k] = target[k];
        }
        return moved;
    }

    void recompute()
    {
        const auto& ps = info().params;
        const double* p = cur;
        hpOn = ! ps[0].isOff (p[0]); lpOn = ! ps[1].isOff (p[1]);
        if (hpOn) f[0].highPass (fs, p[0], 0.707);
        if (lpOn) f[1].lowPass (fs, p[1], 0.707);
        f[2].lowShelf (fs, p[3], 0.65, p[2]);
        // proportional-Q bells: tighter as gain rises, like analog console bands
        auto propQ = [] (double q, double g) { return q * (0.75 + 0.5 * std::min (1.0, std::abs (g) / 12.0)); };
        f[3].peak (fs, p[5], propQ (p[6], p[4]), p[4]);
        f[4].peak (fs, p[8], propQ (p[9], p[7]), p[7]);
        f[5].highShelf (fs, p[11], 0.65, p[10]);
        active[0] = hpOn; active[1] = lpOn;
        active[2] = std::abs (p[2]) > 1e-6; active[3] = std::abs (p[4]) > 1e-6;
        active[4] = std::abs (p[7]) > 1e-6; active[5] = std::abs (p[10]) > 1e-6;
    }

    double fs = 44100, glide = 0.03, cur[12] {}, target[12] {};
    bool primed = false, hpOn = false, lpOn = false, active[6] {};
    Biquad f[6];
};

//==============================================================================
class PassiveTubeEQ final : public ModuleBase
{
public:
    static const ModuleInfo& staticInfo()
    {
        static const ModuleInfo i = []
        {
            ModuleInfo m;
            m.id = "tubeeq"; m.name = "Passive Tube EQ"; m.shortName = "TUBE EQ"; m.category = "EQ";
            m.faceplate = Faceplate::Blue; m.accent = 0xfff0e2b6; m.supportsOversampling = true;
            m.params = {
                pChoice ("Low Freq", "LOW FREQ", { "20 Hz", "30 Hz", "60 Hz", "100 Hz" }, 2),
                pLin ("Low Boost", "LOW BOOST", 0.0, 10.0, 0.0, ""),
                pLin ("Low Atten", "LOW ATTEN", 0.0, 10.0, 0.0, ""),
                pChoice ("High Freq", "HIGH FREQ", { "3 kHz", "4 kHz", "5 kHz", "8 kHz", "10 kHz", "12 kHz", "16 kHz" }, 3),
                pLin ("High Boost", "HI BOOST", 0.0, 10.0, 0.0, ""),
                pLin ("Bandwidth", "BANDWIDTH", 0.0, 10.0, 5.0, ""),
                pChoice ("Atten Freq", "ATTEN SEL", { "5 kHz", "10 kHz", "20 kHz" }, 1),
                pLin ("High Atten", "HI ATTEN", 0.0, 10.0, 0.0, ""),
                pLin ("Tube Drive", "TUBE", 0.0, 10.0, 2.0, ""),
                pLin ("Output", "OUTPUT", -12.0, 6.0, 0.0, "dB"),
            };
            return m;
        }();
        return i;
    }
    const ModuleInfo& info() const override { return staticInfo(); }

    void prepare (double newFs, int, int) override
    {
        fs = newFs; dc.prepare (fs); outS.prepare (fs); drvS.prepare (fs);
        glide = 1.0 - std::exp (-(double) kChunk / (0.025 * fs));
        reset();
    }
    void reset() override { for (auto& b : f) b.reset(); dc.reset(); outS.snap (dbToGain (outDb)); drvS.snap (drive); primed = false; }

    void setParams (const double* p) override
    {
        drive = p[8]; outDb = p[9];
        drvS.set (drive); outS.set (dbToGain (outDb));
        for (int k = 0; k < 8; ++k) target[k] = p[k];
        for (int k : { 0, 3, 6 }) cur[k] = p[k];           // frequency selectors switch instantly
        if (! primed) { for (int k = 0; k < 8; ++k) cur[k] = target[k]; recompute(); primed = true; }
        else if (cur[0] != lastSel[0] || cur[3] != lastSel[1] || cur[6] != lastSel[2]) recompute();
    }

    void recompute()
    {
        const double* p = cur;
        lastSel[0] = p[0]; lastSel[1] = p[3]; lastSel[2] = p[6];
        static const double lowF[] = { 20, 30, 60, 100 }, hiF[] = { 3000, 4000, 5000, 8000, 10000, 12000, 16000 }, atF[] = { 5000, 10000, 20000 };
        const double lf = lowF[(int) p[0]];
        f[0].lowShelf (fs, lf, 0.55, p[1] * 1.35);          // boost
        f[1].lowShelf (fs, lf * 1.7, 0.6, -p[2] * 1.15);    // attenuation sits a touch higher: the classic bump/dip
        const double q = 0.45 + (10.0 - p[5]) * 0.26;
        f[2].peak (fs, hiF[(int) p[3]], q, p[4] * 1.6);
        f[3].highShelf (fs, std::min (atF[(int) p[6]], fs * 0.45), 0.7, -p[7] * 1.6);
    }

    bool step()
    {
        bool moved = false;
        for (int k : { 1, 2, 4, 5, 7 })
        {
            if (cur[k] == target[k]) continue;
            moved = true;
            cur[k] += (target[k] - cur[k]) * glide;
            if (std::abs (cur[k] - target[k]) < 1.0e-4) cur[k] = target[k];
        }
        return moved;
    }

    void process (double* l, double* r, int n) override
    {
        constexpr double h = 0.22;
        double* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            if (i % kChunk == 0 && step()) recompute();
            const double d = drvS.next(), o = outS.next();
            const double g = 1.0 + d * 0.5, b = 0.02 + d * 0.012, sb = softClip (b), hg = h * g;
            const double norm = 1.0 / (hg * (1.0 - b * b * 0.33));
            for (int c = 0; c < 2; ++c)
            {
                double x = ch[c][i];
                for (auto& bq : f) x = bq.process (x, c);
                x = dc.process ((softClip (hg * x + b) - sb) * norm, c);
                ch[c][i] = x * o;
            }
        }
    }

private:
    static constexpr int kChunk = 32;
    double fs = 44100, drive = 2, outDb = 0, glide = 0.03, cur[8] {}, target[8] {}, lastSel[3] { -1, -1, -1 };
    bool primed = false;
    Biquad f[4]; DCBlocker dc; Smoother outS, drvS;
};
} // namespace bl
