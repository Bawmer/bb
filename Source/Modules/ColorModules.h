#pragma once
// Console Preamp, Tape Saturator (oversampling-capable color modules)
#include "Engine/ModuleBase.h"

namespace bl
{
//==============================================================================
class ConsolePreamp final : public ModuleBase
{
public:
    static const ModuleInfo& staticInfo()
    {
        static const ModuleInfo i = []
        {
            ModuleInfo m;
            m.id = "preamp"; m.name = "Console Preamp"; m.shortName = "PRE"; m.category = "Color";
            m.faceplate = Faceplate::Silver; m.accent = 0xffd9822b; m.supportsOversampling = true;
            m.params = {
                pLin ("Drive", "DRIVE", 0.0, 24.0, 0.0, "dB"),
                pLin ("Tilt", "TILT", -6.0, 6.0, 0.0, "dB"),
                pChoice ("Color", "COLOR", { "Clean", "Iron", "Tube" }, 1),
                withOffMin (pLog ("Low Cut", "LOW CUT", 20.0, 250.0, 20.0, "Hz")),
                pLin ("Output", "OUTPUT", -18.0, 6.0, 0.0, "dB"),
            };
            return m;
        }();
        return i;
    }
    const ModuleInfo& info() const override { return staticInfo(); }

    void prepare (double newFs, int, int) override
    {
        fs = newFs; outS.prepare (fs); driveS.prepare (fs); dc.prepare (fs);
        ironLp.lowPass (fs, 160.0, 0.6); airLp.lowPass (fs, 28000.0, 0.7);
        last[0] = -999; reset();
    }
    void reset() override
    {
        tiltLo.reset(); tiltHi.reset(); lowCut.reset(); ironLp.reset(); airLp.reset(); dc.reset();
        outS.snap (dbToGain (outDb)); driveS.snap (dbToGain (driveDb));
    }
    void setParams (const double* p) override
    {
        driveDb = p[0]; color = (int) p[2]; outDb = p[4];
        if (p[1] != last[0] || p[3] != last[1])
        {
            last[0] = p[1]; last[1] = p[3];
            tiltLo.lowShelf (fs, 900.0, 0.5, -p[1] * 0.5);
            tiltHi.highShelf (fs, 900.0, 0.5, p[1] * 0.5);
            lowCutOn = ! info().params[3].isOff (p[3]);
            if (lowCutOn) lowCut.highPass (fs, p[3], 0.707);
        }
        driveS.set (dbToGain (driveDb)); outS.set (dbToGain (outDb));
    }
    void process (double* l, double* r, int n) override
    {
        constexpr double h = 0.25;
        double* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const double g = driveS.next(), o = outS.next(), hg = h * g;
            for (int c = 0; c < 2; ++c)
            {
                double x = ch[c][i];
                if (lowCutOn) x = lowCut.process (x, c);
                double y;
                if (color == 0)
                    y = softClip (hg * x) / hg;
                else if (color == 1)
                {
                    const double lf = ironLp.process (x, c);
                    const double v = hg * x, e = hg * lf;
                    y = (v + (softClip (e * 1.6) / 1.6 - e)) / hg;
                    y = softClip (y * h) / h;
                    y = dc.process (airLp.process (y, c), c);
                }
                else
                {
                    constexpr double b = 0.18;
                    y = (softClip (hg * x + b) - softClip (b)) / (hg * 0.9445);
                    y = dc.process (y, c);
                }
                y = tiltHi.process (tiltLo.process (y, c), c);
                ch[c][i] = y * o;
            }
        }
    }

private:
    double fs = 44100, driveDb = 0, outDb = 0, last[2] { -999, -999 };
    int color = 1; bool lowCutOn = false;
    Biquad tiltLo, tiltHi, lowCut, ironLp, airLp;
    DCBlocker dc; Smoother outS, driveS;
};

//==============================================================================
class TapeSaturator final : public ModuleBase
{
public:
    static const ModuleInfo& staticInfo()
    {
        static const ModuleInfo i = []
        {
            ModuleInfo m;
            m.id = "tape"; m.name = "Tape Saturator"; m.shortName = "TAPE"; m.category = "Color";
            m.faceplate = Faceplate::Bronze; m.accent = 0xffe8b04a; m.supportsOversampling = true;
            m.params = {
                pLin ("Drive", "DRIVE", -12.0, 18.0, 0.0, "dB"),
                pLin ("Bias", "BIAS", -100.0, 100.0, 0.0, "%", 0),
                pLin ("Head Bump", "BUMP", 0.0, 100.0, 50.0, "%", 0),
                pChoice ("Speed", "SPEED", { "7.5 ips", "15 ips", "30 ips" }, 1),
                pLin ("Wow/Flutter", "WOW", 0.0, 100.0, 0.0, "%", 0),
                pToggle ("Hiss", "HISS", false),
                pLin ("Output", "OUTPUT", -18.0, 12.0, 0.0, "dB"),
            };
            return m;
        }();
        return i;
    }
    const ModuleInfo& info() const override { return staticInfo(); }

    void prepare (double newFs, int, int osFactor) override
    {
        fs = newFs; os = std::max (1, osFactor);
        const int baseD = (int) std::ceil (0.0015 * fs / os);
        D = baseD * os;
        delay.prepare (D * 2 + 8);
        pre.highShelf (fs, 3000.0, 0.7, 5.0); de.highShelf (fs, 3000.0, 0.7, -5.0);
        dc.prepare (fs); inS.prepare (fs); outS.prepare (fs);
        hissLp.lowPass (fs, 7000.0, 0.5);
        for (auto& v : last) v = -999;
        reset();
    }
    int latency() const override { return D; }
    void reset() override
    {
        pre.reset(); de.reset(); bump.reset(); dip.reset(); hf.reset(); biasShelf.reset(); hissLp.reset();
        dc.reset(); delay.reset(); phW = phF = 0; drift = 0;
        inS.snap (dbToGain (driveDb)); outS.snap (dbToGain (outDb));
    }
    void setParams (const double* p) override
    {
        driveDb = p[0]; bias = p[1] / 100.0; wow = p[4] / 100.0; hiss = p[5] > 0.5; outDb = p[6];
        speed = (int) p[3];
        if (p[1] != last[0] || p[2] != last[1] || p[3] != last[2])
        {
            last[0] = p[1]; last[1] = p[2]; last[2] = p[3];
            static const double bumpF[] = { 48.0, 72.0, 105.0 }, hfF[] = { 12500.0, 18000.0, 24000.0 };
            const double amt = p[2] / 100.0;
            bump.peak (fs, bumpF[speed], 1.1, amt * 4.0);
            dip.peak (fs, bumpF[speed] * 2.3, 1.3, -amt * 1.5);
            hf.lowPass (fs, std::min (hfF[speed], fs * 0.45), 0.6);
            biasShelf.highShelf (fs, 8000.0, 0.7, -bias * 3.0);
        }
        inS.set (dbToGain (driveDb)); outS.set (dbToGain (outDb));
    }
    void process (double* l, double* r, int n) override
    {
        constexpr double h = 0.3;
        static const double hissDb[] = { -58.0, -64.0, -69.0 };
        const double hissG = dbToGain (hissDb[speed]) * 3.0;
        const double a = bias * 0.12, k = 1.0 + std::max (0.0, -bias) * 0.6;
        const double sa = softClip (a);
        const double wowDepth = wow * 0.00055 * fs, flutDepth = wow * 0.00007 * fs;
        const double dW = 2 * kPi * 0.65 / fs, dF = 2 * kPi * 6.3 / fs, driftC = timeCoef (0.4, fs);
        double* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const double g = inS.next(), o = outS.next();
            double y[2];
            for (int c = 0; c < 2; ++c)
            {
                double x = pre.process (ch[c][i] * g, c);
                const double u = h * x;
                double s = (softClip (k * u + a) - sa) / (k * h);
                s = dc.process (de.process (s, c), c);
                s = biasShelf.process (hf.process (dip.process (bump.process (s, c), c), c), c);
                y[c] = s / g;
            }
            delay.push (y[0], y[1]);
            double rd = (double) D;
            if (wow > 0.0)
            {
                drift = noise.next() * (1.0 - driftC) + drift * driftC;
                phW += dW; if (phW > 2 * kPi) phW -= 2 * kPi;
                phF += dF; if (phF > 2 * kPi) phF -= 2 * kPi;
                rd += wowDepth * (std::sin (phW) + drift * 8.0) * 0.5 + flutDepth * std::sin (phF);
                rd = clampd (rd, 2.0, 2.0 * D);
            }
            for (int c = 0; c < 2; ++c)
            {
                double s = wow > 0.0 ? delay.readFrac (c, rd) : delay.read (c, D);
                if (hiss) s += hissLp.process (noise.next(), c) * hissG;
                ch[c][i] = s * o;
            }
        }
    }

private:
    double fs = 44100, driveDb = 0, outDb = 0, bias = 0, wow = 0, phW = 0, phF = 0, drift = 0, last[3] {};
    int os = 1, D = 64, speed = 1; bool hiss = false;
    Biquad pre, de, bump, dip, hf, biasShelf, hissLp;
    DCBlocker dc; Smoother inS, outS; DelayLine delay; Noise noise;
};
} // namespace bl
