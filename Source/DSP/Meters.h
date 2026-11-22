#pragma once
// Peak / RMS / short-term loudness (ITU-R BS.1770 K-weighting, 3 s window)
#include "DSP/Common.h"
#include <atomic>

namespace bl
{
struct KWeighting
{
    Biquad shelf, hp;
    void prepare (double fs)
    {
        // pre-filter (high shelf) and RLB high-pass, generalised for any sample rate
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (kPi * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.b0 = (Vh + Vb * K / Q + K * K) / a0; shelf.b1 = 2.0 * (K * K - Vh) / a0; shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
            shelf.a1 = 2.0 * (K * K - 1.0) / a0; shelf.a2 = (1.0 - K / Q + K * K) / a0;
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773, K = std::tan (kPi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            hp.b0 = 1.0; hp.b1 = -2.0; hp.b2 = 1.0;
            hp.a1 = 2.0 * (K * K - 1.0) / a0; hp.a2 = (1.0 - K / Q + K * K) / a0;
        }
        shelf.reset(); hp.reset();
    }
    inline double process (double x, int c) noexcept { return hp.process (shelf.process (x, c), c); }
};

class LevelMeter
{
public:
    void prepare (double newFs)
    {
        fs = newFs; kw.prepare (fs);
        blockLen = std::max (1, (int) (fs * 0.1));
        rmsC = timeCoef (0.3, fs);
        reset();
    }
    void reset()
    {
        acc = 0; accN = 0; ringPos = 0; ringFill = 0; ms[0] = ms[1] = 0;
        for (auto& b : ring) b = 0;
        peak[0] = peak[1] = 0.0f; rms[0] = rms[1] = 0.0f; lufs = -100.0f;
    }

    /** numChannels = 1 meters (and weighs) only the left signal, as BS.1770 does for mono. */
    void process (const double* l, const double* r, int n, int numChannels = 2) noexcept
    {
        const int nc = numChannels < 2 ? 1 : 2;
        double pk[2] { 0, 0 };
        for (int i = 0; i < n; ++i)
        {
            const double x[2] = { l[i], nc == 1 ? l[i] : r[i] };
            for (int c = 0; c < nc; ++c)
            {
                pk[c] = std::max (pk[c], std::abs (x[c]));
                ms[c] = x[c] * x[c] + (ms[c] - x[c] * x[c]) * rmsC;
                const double k = kw.process (x[c], c);
                acc += k * k;
            }
            if (++accN >= blockLen)
            {
                ring[ringPos] = acc / accN; ringPos = (ringPos + 1) % 30; ringFill = std::min (30, ringFill + 1);
                acc = 0; accN = 0;
                double s = 0; for (int b = 0; b < ringFill; ++b) s += ring[b];
                const double meanSq = s / std::max (1, ringFill);
                lufs.store ((float) (meanSq > 1e-12 ? -0.691 + 10.0 * std::log10 (meanSq) : -100.0));
            }
        }
        if (nc == 1) { pk[1] = pk[0]; ms[1] = ms[0]; }
        for (int c = 0; c < 2; ++c)
        {
            float cur = peak[c].load();
            while ((float) pk[c] > cur && ! peak[c].compare_exchange_weak (cur, (float) pk[c])) {}
            rms[c].store ((float) std::sqrt (ms[c]));
        }
    }

    /** GUI: read and clear the peak since last call. */
    float takePeak (int c) noexcept { return peak[c].exchange (0.0f); }
    float getRms (int c) const noexcept { return rms[c].load(); }
    float getLufsS() const noexcept { return lufs.load(); }

private:
    double fs = 44100, rmsC = 0, acc = 0, ms[2] {}, ring[30] {};
    int blockLen = 4410, accN = 0, ringPos = 0, ringFill = 0;
    KWeighting kw;
    std::atomic<float> peak[2] { {0.0f}, {0.0f} }, rms[2] { {0.0f}, {0.0f} }, lufs { -100.0f };
};
} // namespace bl
