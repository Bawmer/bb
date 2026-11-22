#pragma once
// De-Esser, Gate/Expander, Stereo Imager, Transient Shaper
#include "Engine/ModuleBase.h"

namespace bl
{
//==============================================================================
class DeEsser final : public ModuleBase
{
public:
    static const ModuleInfo& staticInfo()
    {
        static const ModuleInfo i = []
        {
            ModuleInfo m;
            m.id = "deess"; m.name = "De-Esser"; m.shortName = "DE-ESS"; m.category = "Dynamics";
            m.faceplate = Faceplate::Teal; m.accent = 0xffffd166; m.hasGainReduction = true;
            m.params = {
                pLog ("Frequency", "FREQ", 2000.0, 12000.0, 6000.0, "Hz"),
                pLin ("Threshold", "THRESH", -60.0, 0.0, -30.0, "dB"),
                pLin ("Range", "RANGE", 0.0, 24.0, 8.0, "dB"),
                pChoice ("Mode", "MODE", { "Split", "Wide" }, 0),
                pToggle ("Listen", "LISTEN", false),
            };
            return m;
        }();
        return i;
    }
    const ModuleInfo& info() const override { return staticInfo(); }

    void prepare (double newFs, int, int) override
    {
        fs = newFs; attC = timeCoef (0.0005, fs); relC = timeCoef (0.06, fs); envRel = timeCoef (0.03, fs);
        lastF = -1; reset();
    }
    void reset() override { split.reset(); sc.reset(); gr = 0; env = 0; reportGr (0); }
    void setParams (const double* p) override
    {
        if (p[0] != lastF) { lastF = p[0]; split.highPass (fs, p[0], 0.707); sc.highPass (fs, p[0] * 0.9, 0.9); }
        thr = p[1]; range = p[2]; wide = p[3] > 0.5; listen = p[4] > 0.5;
    }
    void process (double* l, double* r, int n) override
    {
        double blockMax = 0;
        double* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const double sl = sc.process (l[i], 0), sr = sc.process (r[i], 1);
            const double pk = std::max (std::abs (sl), std::abs (sr));
            env = pk > env ? pk : pk + (env - pk) * envRel;
            const double over = gainToDb (env) - thr;
            const double target = over > 0.0 ? std::min (range, over * 0.75) : 0.0;
            gr = target > gr ? target + (gr - target) * attC : target + (gr - target) * relC;
            const double g = dbToGain (-gr);
            for (int c = 0; c < 2; ++c)
            {
                const double x = ch[c][i];
                const double band = split.process (x, c);
                if (listen)        ch[c][i] = band;
                else if (wide)     ch[c][i] = x * g;
                else               ch[c][i] = (x - band) + band * g;
            }
            blockMax = std::max (blockMax, gr);
        }
        reportGr (blockMax);
    }

private:
    double fs = 44100, attC = 0, relC = 0, envRel = 0, lastF = -1, thr = -30, range = 8, gr = 0, env = 0;
    bool wide = false, listen = false; Biquad split, sc;
};

//==============================================================================
class GateExpander final : public ModuleBase
{
public:
    static const ModuleInfo& staticInfo()
    {
        static const ModuleInfo i = []
        {
            ModuleInfo m;
            m.id = "gate"; m.name = "Gate / Expander"; m.shortName = "GATE"; m.category = "Dynamics";
            m.faceplate = Faceplate::Black; m.accent = 0xff6ee06e; m.hasGainReduction = true;
            m.params = {
                pLin ("Threshold", "THRESH", -80.0, 0.0, -60.0, "dB"),
                pLin ("Range", "RANGE", 0.0, 80.0, 40.0, "dB", 0),
                pChoice ("Mode", "MODE", { "Gate", "Exp 1:2", "Exp 1:4" }, 0),
                pLog ("Attack", "ATTACK", 0.05, 50.0, 0.5, "ms", 2),
                pLin ("Hold", "HOLD", 0.0, 500.0, 30.0, "ms", 0),
                pLog ("Release", "RELEASE", 5.0, 2000.0, 120.0, "ms"),
                pLin ("Hysteresis", "HYST", 0.0, 12.0, 3.0, "dB"),
                withOffMin (pLog ("SC High Pass", "SC HPF", 20.0, 2000.0, 20.0, "Hz")),
                withOffMax (pLog ("SC Low Pass", "SC LPF", 1000.0, 20000.0, 20000.0, "Hz")),
            };
            return m;
        }();
        return i;
    }
    const ModuleInfo& info() const override { return staticInfo(); }

    void prepare (double newFs, int, int) override { fs = newFs; envRel = timeCoef (0.008, fs); lastHp = lastLp = -1; reset(); }
    void reset() override { hp.reset(); lp.reset(); env = 0; gDb = 0; open = true; holdLeft = 0; reportGr (0); }
    void setParams (const double* p) override
    {
        const auto& ps = info().params;
        thr = p[0]; range = p[1]; mode = (int) p[2];
        attC = timeCoef (p[3] * 0.001, fs); holdN = (int) (p[4] * 0.001 * fs); relC = timeCoef (p[5] * 0.001, fs); hyst = p[6];
        if (p[7] != lastHp) { lastHp = p[7]; hpOn = ! ps[7].isOff (p[7]); if (hpOn) hp.highPass (fs, p[7], 0.707); }
        if (p[8] != lastLp) { lastLp = p[8]; lpOn = ! ps[8].isOff (p[8]); if (lpOn) lp.lowPass (fs, p[8], 0.707); }
    }
    void process (double* l, double* r, int n) override
    {
        double blockMax = 0;
        const double ratio = mode == 1 ? 2.0 : 4.0;
        for (int i = 0; i < n; ++i)
        {
            double a = l[i], b = r[i];
            if (hpOn) { a = hp.process (a, 0); b = hp.process (b, 1); }
            if (lpOn) { a = lp.process (a, 0); b = lp.process (b, 1); }
            const double pk = std::max (std::abs (a), std::abs (b));
            env = pk > env ? pk : pk + (env - pk) * envRel;
            const double lv = gainToDb (env);
            double target;
            if (mode == 0)
            {
                if (lv >= thr) { open = true; holdLeft = holdN; }
                else if (open && lv < thr - hyst) { if (holdLeft > 0) --holdLeft; else open = false; }
                target = open ? 0.0 : -range;
            }
            else
            {
                const double below = (thr - hyst * 0.5) - lv;
                target = below > 0.0 ? -std::min (range, below * (ratio - 1.0)) : 0.0;
            }
            gDb = target > gDb ? target + (gDb - target) * attC : target + (gDb - target) * relC;
            const double g = dbToGain (gDb);
            l[i] *= g; r[i] *= g;
            blockMax = std::max (blockMax, -gDb);
        }
        reportGr (blockMax);
    }

private:
    double fs = 44100, envRel = 0, thr = -60, range = 40, attC = 0, relC = 0, hyst = 3, env = 0, gDb = 0, lastHp = -1, lastLp = -1;
    int mode = 0, holdN = 0, holdLeft = 0; bool open = true, hpOn = false, lpOn = false;
    Biquad hp, lp;
};

//==============================================================================
class StereoImager final : public ModuleBase
{
public:
    static const ModuleInfo& staticInfo()
    {
        static const ModuleInfo i = []
        {
            ModuleInfo m;
            m.id = "imager"; m.name = "Stereo Imager"; m.shortName = "IMAGE"; m.category = "Utility";
            m.faceplate = Faceplate::Teal; m.accent = 0xff7fe0ff;
            m.params = {
                pLin ("Width", "WIDTH", 0.0, 200.0, 100.0, "%", 0),
                withOffMin (pLog ("Mono Bass", "MONO BASS", 20.0, 300.0, 20.0, "Hz")),
                pLin ("M/S Balance", "M/S BAL", -100.0, 100.0, 0.0, "%", 0),
                pLin ("Output", "OUTPUT", -12.0, 12.0, 0.0, "dB"),
            };
            return m;
        }();
        return i;
    }
    const ModuleInfo& info() const override { return staticInfo(); }

    void prepare (double newFs, int, int) override { fs = newFs; wS.prepare (fs); mS.prepare (fs); sS.prepare (fs); oS.prepare (fs); lastMb = -1; reset(); }
    void reset() override { hp1.reset(); hp2.reset(); wS.snap (width); mS.snap (mg); sS.snap (sg); oS.snap (og); }
    void setParams (const double* p) override
    {
        width = p[0] / 100.0; mg = dbToGain (p[2] * 0.06); sg = dbToGain (-p[2] * 0.06); og = dbToGain (p[3]);
        wS.set (width); mS.set (mg); sS.set (sg); oS.set (og);
        if (p[1] != lastMb) { lastMb = p[1]; mbOn = ! info().params[1].isOff (p[1]); if (mbOn) { hp1.highPass (fs, p[1], 0.7071); hp2.highPass (fs, p[1], 0.7071); } }
    }
    void process (double* l, double* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            const double w = wS.next(), m = mS.next(), s = sS.next(), o = oS.next();
            double M = (l[i] + r[i]) * 0.5, S = (l[i] - r[i]) * 0.5;
            if (mbOn) S = hp2.process (hp1.process (S, 0), 0);
            M *= m * o; S *= w * s * o;
            l[i] = M + S; r[i] = M - S;
        }
    }

private:
    double fs = 44100, width = 1, mg = 1, sg = 1, og = 1, lastMb = -1; bool mbOn = false;
    Biquad hp1, hp2; Smoother wS, mS, sS, oS;
};

//==============================================================================
class TransientShaper final : public ModuleBase
{
public:
    static const ModuleInfo& staticInfo()
    {
        static const ModuleInfo i = []
        {
            ModuleInfo m;
            m.id = "transient"; m.name = "Transient Shaper"; m.shortName = "TRANS"; m.category = "Dynamics";
            m.faceplate = Faceplate::Red; m.accent = 0xfff4f4f4;
            m.params = {
                pLin ("Attack", "ATTACK", -100.0, 100.0, 0.0, "%", 0),
                pLin ("Sustain", "SUSTAIN", -100.0, 100.0, 0.0, "%", 0),
                pChoice ("Speed", "SPEED", { "Fast", "Medium", "Slow" }, 1),
                pLin ("Output", "OUTPUT", -12.0, 12.0, 0.0, "dB"),
            };
            return m;
        }();
        return i;
    }
    const ModuleInfo& info() const override { return staticInfo(); }

    void prepare (double newFs, int, int) override { fs = newFs; oS.prepare (fs); gS.prepare (fs, 0.001); reset(); }
    void reset() override { fast = slow = 0; oS.snap (dbToGain (outDb)); gS.snap (1.0); }
    void setParams (const double* p) override
    {
        static const double sm[] = { 0.6, 1.0, 1.8 };
        atk = p[0] / 100.0; sus = p[1] / 100.0; outDb = p[3];
        const double k = sm[(int) p[2]];
        fA = timeCoef (0.0008 * k, fs); fR = timeCoef (0.025 * k, fs);
        sA = timeCoef (0.018 * k, fs);  sR = timeCoef (0.180 * k, fs);
        oS.set (dbToGain (outDb));
    }
    void process (double* l, double* r, int n) override
    {
        const bool neutral = std::abs (atk) < 1e-6 && std::abs (sus) < 1e-6;
        for (int i = 0; i < n; ++i)
        {
            const double pk = std::max (std::abs (l[i]), std::abs (r[i]));
            fast = pk + (fast - pk) * (pk > fast ? fA : fR);
            slow = pk + (slow - pk) * (pk > slow ? sA : sR);
            double gainDb = 0.0;
            if (! neutral)
            {
                const double d = gainToDb (fast + 1e-9) - gainToDb (slow + 1e-9);
                gainDb = atk * clampd (d, 0.0, 24.0) * 0.8 + sus * clampd (-d, 0.0, 24.0) * 0.8;
                gainDb = clampd (gainDb, -18.0, 18.0);
            }
            gS.set (dbToGain (gainDb));
            const double g = gS.next() * oS.next();
            l[i] *= g; r[i] *= g;
        }
    }

private:
    double fs = 44100, atk = 0, sus = 0, outDb = 0, fast = 0, slow = 0, fA = 0, fR = 0, sA = 0, sR = 0;
    Smoother oS, gS;
};
} // namespace bl
