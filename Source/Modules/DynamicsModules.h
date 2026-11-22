#pragma once
// FET Compressor, Opto Compressor, VCA Bus Compressor
#include "Engine/ModuleBase.h"

namespace bl
{
/** Soft-knee static curve: returns gain reduction in dB (>= 0). */
inline double kneeGr (double levelDb, double thrDb, double ratio, double kneeDb) noexcept
{
    const double over = levelDb - thrDb, slope = 1.0 - 1.0 / ratio;
    if (kneeDb > 0.0 && 2.0 * std::abs (over) <= kneeDb)
    {
        const double t = over + kneeDb * 0.5;
        return slope * t * t / (2.0 * kneeDb);
    }
    return over > 0.0 ? slope * over : 0.0;
}

//==============================================================================
class FETCompressor final : public ModuleBase
{
public:
    static const ModuleInfo& staticInfo()
    {
        static const ModuleInfo i = []
        {
            ModuleInfo m;
            m.id = "fet"; m.name = "FET Compressor"; m.shortName = "FET"; m.category = "Dynamics";
            m.faceplate = Faceplate::Silver; m.accent = 0xffffb347; m.supportsOversampling = true; m.hasGainReduction = true;
            m.params = {
                pLin ("Input", "INPUT", 0.0, 48.0, 0.0, "dB"),
                pLin ("Output", "OUTPUT", -24.0, 24.0, 0.0, "dB"),
                pLog ("Attack", "ATTACK", 0.02, 0.8, 0.2, "ms", 2),
                pLog ("Release", "RELEASE", 50.0, 1100.0, 400.0, "ms"),
                pChoice ("Ratio", "RATIO", { "4:1", "8:1", "12:1", "20:1", "All" }, 0),
            };
            return m;
        }();
        return i;
    }
    const ModuleInfo& info() const override { return staticInfo(); }

    void prepare (double newFs, int, int) override { fs = newFs; inS.prepare (fs); outS.prepare (fs); reset(); }
    void reset() override { gr = 0; inS.snap (dbToGain (inDb)); outS.snap (dbToGain (outDb)); reportGr (0); }
    void setParams (const double* p) override
    {
        inDb = p[0]; outDb = p[1]; ratioIdx = (int) p[4];
        attC = timeCoef (p[2] * 0.001, fs); relC = timeCoef (p[3] * 0.001, fs);
        inS.set (dbToGain (inDb)); outS.set (dbToGain (outDb));
    }
    void process (double* l, double* r, int n) override
    {
        static const double ratios[] = { 4, 8, 12, 20, 40 };
        const double ratio = ratios[ratioIdx];
        const bool all = ratioIdx == 4;
        const double knee = all ? 2.0 : 5.0, thr = all ? -20.0 : -18.0;
        const double colorH = all ? 0.85 : 0.45;
        const double aC = all ? attC * 0.6 : attC, rC = all ? std::pow (relC, 0.8) : relC;
        double blockMax = 0;
        for (int i = 0; i < n; ++i)
        {
            const double g = inS.next(), o = outS.next();
            const double a = l[i] * g, b = r[i] * g;
            const double lv = gainToDb (std::max (std::abs (a), std::abs (b)));
            const double target = kneeGr (lv, thr, ratio, knee);
            gr = target > gr ? target + (gr - target) * aC : target + (gr - target) * rC;
            const double comp = dbToGain (-gr) * colorH;
            l[i] = softClip (a * comp) / colorH * o;
            r[i] = softClip (b * comp) / colorH * o;
            blockMax = std::max (blockMax, gr);
        }
        reportGr (blockMax);
    }

private:
    double fs = 44100, inDb = 0, outDb = 0, attC = 0, relC = 0, gr = 0;
    int ratioIdx = 0; Smoother inS, outS;
};

//==============================================================================
class OptoCompressor final : public ModuleBase
{
public:
    static const ModuleInfo& staticInfo()
    {
        static const ModuleInfo i = []
        {
            ModuleInfo m;
            m.id = "opto"; m.name = "Opto Compressor"; m.shortName = "OPTO"; m.category = "Dynamics";
            m.faceplate = Faceplate::Cream; m.accent = 0xff2b2b2b; m.hasGainReduction = true;
            m.params = {
                pLin ("Peak Reduction", "PEAK RED", 0.0, 100.0, 0.0, "", 0),
                pLin ("Gain", "GAIN", 0.0, 40.0, 0.0, "dB"),
                pChoice ("Mode", "MODE", { "Compress", "Limit" }, 0),
                pLin ("Emphasis", "EMPHASIS", 0.0, 100.0, 0.0, "%", 0),
            };
            return m;
        }();
        return i;
    }
    const ModuleInfo& info() const override { return staticInfo(); }

    void prepare (double newFs, int, int) override
    {
        fs = newFs; gainS.prepare (fs);
        attC = timeCoef (0.010, fs); rmsC = timeCoef (0.004, fs);
        chargeC = 1.0 / (1.2 * fs); dischargeC = 1.0 / (4.0 * fs);
        lastEmph = -1; reset();
    }
    void reset() override { gr = 0; mem = 0; env = 0; emph.reset(); gainS.snap (dbToGain (gainDb)); reportGr (0); }
    void setParams (const double* p) override
    {
        thr = -p[0] * 0.45; gainDb = p[1]; limit = p[2] > 0.5;
        if (p[3] != lastEmph) { lastEmph = p[3]; emph.highShelf (fs, 2500.0, 0.6, p[3] * 0.1); }
        gainS.set (dbToGain (gainDb));
    }
    void process (double* l, double* r, int n) override
    {
        const double ratio = limit ? 12.0 : 3.0, knee = limit ? 4.0 : 10.0;
        const double relC = timeCoef (0.06 + mem * 1.8, fs);
        double blockMax = 0;
        for (int i = 0; i < n; ++i)
        {
            const double sl = emph.process (l[i], 0), sr = emph.process (r[i], 1);
            const double p2 = std::max (sl * sl, sr * sr);
            env = p2 + (env - p2) * rmsC;
            const double lv = 10.0 * std::log10 (std::max (env * 2.0, 1e-24));
            const double target = kneeGr (lv, thr, ratio, knee);
            gr = target > gr ? target + (gr - target) * attC : target + (gr - target) * relC;
            mem += gr > 2.5 ? (1.0 - mem) * chargeC : -mem * dischargeC;
            const double g = dbToGain (-gr) * gainS.next();
            l[i] *= g; r[i] *= g;
            blockMax = std::max (blockMax, gr);
        }
        reportGr (blockMax);
    }

private:
    double fs = 44100, thr = 0, gainDb = 0, attC = 0, rmsC = 0, chargeC = 0, dischargeC = 0;
    double gr = 0, mem = 0, env = 0, lastEmph = -1; bool limit = false;
    Biquad emph; Smoother gainS;
};

//==============================================================================
class VCABusCompressor final : public ModuleBase
{
public:
    static const ModuleInfo& staticInfo()
    {
        static const ModuleInfo i = []
        {
            ModuleInfo m;
            m.id = "vca"; m.name = "VCA Bus Comp"; m.shortName = "VCA"; m.category = "Dynamics";
            m.faceplate = Faceplate::Graphite; m.accent = 0xffe23b3b; m.hasGainReduction = true;
            m.params = {
                pLin ("Threshold", "THRESH", -40.0, 0.0, 0.0, "dB"),
                pChoice ("Ratio", "RATIO", { "1.5:1", "2:1", "4:1", "10:1" }, 1),
                pChoice ("Attack", "ATTACK", { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }, 4),
                pChoice ("Release", "RELEASE", { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "Auto" }, 4),
                pLin ("Makeup", "MAKEUP", 0.0, 20.0, 0.0, "dB"),
                withOffMin (pLog ("SC High Pass", "SC HPF", 20.0, 400.0, 20.0, "Hz")),
            };
            return m;
        }();
        return i;
    }
    const ModuleInfo& info() const override { return staticInfo(); }

    void prepare (double newFs, int, int) override { fs = newFs; mkS.prepare (fs); lastHp = -1; reset(); }
    void reset() override { gr = 0; slowGr = 0; hp.reset(); mkS.snap (dbToGain (mkDb)); reportGr (0); }
    void setParams (const double* p) override
    {
        static const double ratios[] = { 1.5, 2, 4, 10 }, att[] = { 0.0001, 0.0003, 0.001, 0.003, 0.01, 0.03 }, rel[] = { 0.1, 0.3, 0.6, 1.2 };
        thr = p[0]; ratio = ratios[(int) p[1]]; attC = timeCoef (att[(int) p[2]], fs);
        autoRel = (int) p[3] == 4; if (! autoRel) relC = timeCoef (rel[(int) p[3]], fs);
        mkDb = p[4]; mkS.set (dbToGain (mkDb));
        if (p[5] != lastHp) { lastHp = p[5]; hpOn = ! info().params[5].isOff (p[5]); if (hpOn) hp.highPass (fs, p[5], 0.707); }
    }
    void process (double* l, double* r, int n) override
    {
        if (autoRel) relC = timeCoef (0.1 + 1.1 * clampd (slowGr / 6.0, 0.0, 1.0), fs);
        const double slowC = timeCoef (0.8, fs);
        double blockMax = 0;
        for (int i = 0; i < n; ++i)
        {
            double a = l[i], b = r[i];
            if (hpOn) { a = hp.process (a, 0); b = hp.process (b, 1); }
            const double lv = gainToDb (std::max (std::abs (a), std::abs (b)));
            const double target = kneeGr (lv, thr, ratio, 4.0);
            gr = target > gr ? target + (gr - target) * attC : target + (gr - target) * relC;
            slowGr = gr + (slowGr - gr) * slowC;
            const double g = dbToGain (-gr) * mkS.next();
            l[i] *= g; r[i] *= g;
            blockMax = std::max (blockMax, gr);
        }
        reportGr (blockMax);
    }

private:
    double fs = 44100, thr = 0, ratio = 2, attC = 0, relC = 0, mkDb = 0, gr = 0, slowGr = 0, lastHp = -1;
    bool autoRel = true, hpOn = false; Biquad hp; Smoother mkS;
};
} // namespace bl
