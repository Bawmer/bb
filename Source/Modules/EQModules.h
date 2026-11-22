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

    void prepare (double newFs, int, int) override { fs = newFs; for (auto& v : last) v = -999; reset(); }
    void reset() override { for (auto& b : f) b.reset(); }

    void setParams (const double* p) override
    {
        bool changed = false;
        for (int k = 0; k < 12; ++k) if (p[k] != last[k]) { changed = true; last[k] = p[k]; }
        if (! changed) return;
        const auto& ps = info().params;
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

    void process (double* l, double* r, int n) override
    {
        double* ch[2] = { l, r };
        for (int b = 0; b < 6; ++b)
        {
            if (! active[b]) continue;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i) ch[c][i] = f[b].process (ch[c][i], c);
        }
    }

private:
    double fs = 44100, last[12] {};
    bool hpOn = false, lpOn = false, active[6] {};
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

    void prepare (double newFs, int, int) override { fs = newFs; dc.prepare (fs); outS.prepare (fs); drvS.prepare (fs); for (auto& v : last) v = -999; reset(); }
    void reset() override { for (auto& b : f) b.reset(); dc.reset(); outS.snap (dbToGain (outDb)); drvS.snap (drive); }

    void setParams (const double* p) override
    {
        drive = p[8]; outDb = p[9];
        drvS.set (drive); outS.set (dbToGain (outDb));
        bool changed = false;
        for (int k = 0; k < 8; ++k) if (p[k] != last[k]) { changed = true; last[k] = p[k]; }
        if (! changed) return;
        static const double lowF[] = { 20, 30, 60, 100 }, hiF[] = { 3000, 4000, 5000, 8000, 10000, 12000, 16000 }, atF[] = { 5000, 10000, 20000 };
        const double lf = lowF[(int) p[0]];
        f[0].lowShelf (fs, lf, 0.55, p[1] * 1.35);          // boost
        f[1].lowShelf (fs, lf * 1.7, 0.6, -p[2] * 1.15);    // attenuation sits a touch higher: the classic bump/dip
        const double q = 0.45 + (10.0 - p[5]) * 0.26;
        f[2].peak (fs, hiF[(int) p[3]], q, p[4] * 1.6);
        f[3].highShelf (fs, std::min (atF[(int) p[6]], fs * 0.45), 0.7, -p[7] * 1.6);
    }

    void process (double* l, double* r, int n) override
    {
        constexpr double h = 0.22;
        double* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
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
    double fs = 44100, drive = 2, outDb = 0, last[8] {};
    Biquad f[4]; DCBlocker dc; Smoother outS, drvS;
};
} // namespace bl
