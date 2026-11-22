#pragma once
// Output Limiter: look-ahead brickwall with optional true-peak (4x interpolated) detection
#include "Engine/ModuleBase.h"

namespace bl
{
class OutputLimiter final : public ModuleBase
{
public:
    static const ModuleInfo& staticInfo()
    {
        static const ModuleInfo i = []
        {
            ModuleInfo m;
            m.id = "limiter"; m.name = "Output Limiter"; m.shortName = "LIMIT"; m.category = "Dynamics";
            m.faceplate = Faceplate::Black; m.accent = 0xffff8a3d; m.hasGainReduction = true;
            m.params = {
                pLin ("Input", "INPUT", 0.0, 24.0, 0.0, "dB"),
                pLin ("Ceiling", "CEILING", -12.0, 0.0, -0.3, "dB"),
                pLog ("Release", "RELEASE", 1.0, 1000.0, 80.0, "ms"),
                pToggle ("True Peak", "TRUE PEAK", true),
            };
            return m;
        }();
        return i;
    }
    const ModuleInfo& info() const override { return staticInfo(); }

    static constexpr int kTaps = 8;              // per phase
    static constexpr int kHalf = kTaps / 2;      // detector delay (base samples)

    void prepare (double newFs, int, int) override
    {
        fs = newFs;
        L = std::max (1, (int) std::lround (0.0015 * fs));
        cap = 1; while (cap < L + kHalf + kTaps + 8) cap <<= 1;
        hist[0].assign ((size_t) cap, 0.0); hist[1].assign ((size_t) cap, 0.0);
        minQ.assign ((size_t) cap, 0.0); minT.assign ((size_t) cap, 0);
        box.assign ((size_t) cap, 1.0);
        // 4-phase windowed-sinc interpolator for fractional positions 0.25, 0.5, 0.75
        for (int ph = 1; ph < 4; ++ph)
        {
            const double frac = ph / 4.0;
            double sum = 0;
            for (int k = 0; k < kTaps; ++k)
            {
                const double tt = (k - (kHalf - 1)) - frac;   // taps around the point between x[kHalf-1] and x[kHalf]
                const double sinc = std::abs (tt) < 1e-12 ? 1.0 : std::sin (kPi * tt) / (kPi * tt);
                const double win = 0.5 + 0.5 * std::cos (kPi * tt / (kHalf + 0.5));
                coef[ph][k] = sinc * win; sum += coef[ph][k];
            }
            for (int k = 0; k < kTaps; ++k) coef[ph][k] /= sum;
        }
        inS.prepare (fs);
        reset();
    }
    int latency() const override { return L + kHalf; }

    void reset() override
    {
        std::fill (hist[0].begin(), hist[0].end(), 0.0); std::fill (hist[1].begin(), hist[1].end(), 0.0);
        std::fill (box.begin(), box.end(), 1.0);
        w = 0; qHead = qTail = 0; t = 0; boxSum = (double) (L + 1); boxPos = 0; gs = 1.0;
        inS.snap (dbToGain (inDb)); reportGr (0);
    }
    void setParams (const double* p) override
    {
        inDb = p[0]; ceil = dbToGain (p[1]); relC = timeCoef (p[2] * 0.001, fs); tp = p[3] > 0.5;
        inS.set (dbToGain (inDb));
    }
    void process (double* l, double* r, int n) override
    {
        double blockMin = 1.0;
        const int mask = cap - 1, boxLen = L + 1;
        for (int i = 0; i < n; ++i)
        {
            const double g = inS.next();
            hist[0][(size_t) w] = l[i] * g; hist[1][(size_t) w] = r[i] * g;
            // detector at time (w - kHalf)
            double pk = 0;
            for (int c = 0; c < 2; ++c)
            {
                pk = std::max (pk, std::abs (hist[c][(size_t) ((w - kHalf) & mask)]));
                if (tp)
                    for (int ph = 1; ph < 4; ++ph)
                    {
                        double s = 0;
                        for (int k = 0; k < kTaps; ++k)
                            s += coef[ph][k] * hist[c][(size_t) ((w - (kTaps - 1) + k) & mask)];
                        pk = std::max (pk, std::abs (s));
                    }
            }
            const double req = pk > ceil ? ceil / pk : 1.0;

            // sliding minimum over the last L+1 values (monotonic deque)
            while (qTail != qHead && minQ[(size_t) ((qTail - 1) & mask)] >= req) qTail = (qTail - 1) & mask;
            minQ[(size_t) qTail] = req; minT[(size_t) qTail] = t; qTail = (qTail + 1) & mask;
            while (minT[(size_t) qHead] <= t - boxLen) qHead = (qHead + 1) & mask;
            const double held = minQ[(size_t) qHead];

            gs = held < gs ? held : held + (gs - held) * relC;

            boxSum += gs - box[(size_t) boxPos];
            box[(size_t) boxPos] = gs;
            boxPos = boxPos + 1 >= boxLen ? 0 : boxPos + 1;
            const double gain = std::min (1.0, boxSum / boxLen);

            const int d = (w - kHalf - L) & mask;
            l[i] = clampd (hist[0][(size_t) d] * gain, -ceil, ceil);
            r[i] = clampd (hist[1][(size_t) d] * gain, -ceil, ceil);
            blockMin = std::min (blockMin, gain);
            w = (w + 1) & mask; ++t;
        }
        if ((++blockCount & 255) == 0) { boxSum = 0; for (int k = 0; k < L + 1; ++k) boxSum += box[(size_t) k]; }
        reportGr (-gainToDb (blockMin));
    }

private:
    double fs = 44100, inDb = 0, ceil = 0.966, relC = 0, gs = 1, boxSum = 0;
    bool tp = true;
    int L = 66, cap = 256, w = 0, qHead = 0, qTail = 0, boxPos = 0;
    long long t = 0; unsigned blockCount = 0;
    std::vector<double> hist[2], minQ, box; std::vector<long long> minT;
    double coef[4][kTaps] {};
    Smoother inS;
};
} // namespace bl
