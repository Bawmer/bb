// Bub Leon Harmony Stack — YIN pitch tracker (decimated, real-time safe)
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace hs
{

class PitchTracker
{
public:
    static constexpr double kMinF0 = 70.0;
    static constexpr double kMaxF0 = 1000.0;

    void prepare (double sampleRate)
    {
        fs   = sampleRate;
        dec  = std::max (1, (int) std::lround (fs / 24000.0));
        fsd  = fs / dec;
        tauMin = std::max (2, (int) std::floor (fsd / kMaxF0));
        tauMax = (int) std::ceil (fsd / kMinF0) + 2;
        win    = tauMax;
        bufLen = win + tauMax + 2;
        ring.assign ((size_t) bufLen, 0.0f);
        lin.assign  ((size_t) bufLen, 0.0f);
        diff.assign ((size_t) tauMax + 2, 0.0f);
        cmnd.assign ((size_t) tauMax + 2, 0.0f);
        hop = std::max (16, (int) std::lround (fsd * 0.004));

        // RBJ biquad low-pass (anti-alias for decimation + epoch smoothing)
        const double fc = std::min (3500.0, 0.42 * fsd);
        const double w0 = 2.0 * 3.14159265358979323846 * fc / fs, cw = std::cos (w0), sw = std::sin (w0);
        const double q = 0.7071, alpha = sw / (2.0 * q), a0 = 1.0 + alpha;
        b0 = (float) (((1.0 - cw) * 0.5) / a0);
        b1 = (float) ((1.0 - cw) / a0);
        b2 = b0;
        a1 = (float) ((-2.0 * cw) / a0);
        a2 = (float) ((1.0 - alpha) / a0);

        envCoef = (float) std::exp (-1.0 / (0.010 * fs));
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        z1 = z2 = 0.0f;
        writeIdx = decCount = hopCount = 0;
        filled = 0;
        periodSamples = (float) (fs / 200.0);
        voicedFlag = false;
        hangover = 0;
        env = 0.0f;
        hist[0] = hist[1] = hist[2] = periodSamples;
        histN = 0;
    }

    // Push one full-rate sample, returns low-passed sample (used for epoch marking)
    inline float push (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;

        const float ax = std::abs (x);
        env = ax > env ? ax : env * envCoef + ax * (1.0f - envCoef);

        if (++decCount >= dec)
        {
            decCount = 0;
            ring[(size_t) writeIdx] = y;
            writeIdx = (writeIdx + 1) % bufLen;
            if (filled < bufLen) ++filled;
            if (++hopCount >= hop)
            {
                hopCount = 0;
                if (filled >= bufLen) analyse();
            }
        }
        return y;
    }

    float period() const noexcept   { return periodSamples; }   // full-rate samples
    bool  voiced() const noexcept   { return voicedFlag; }
    float midiNote() const noexcept { return (float) (69.0 + 12.0 * std::log2 (fs / (periodSamples * 440.0))); }
    float maxPeriod() const noexcept { return (float) (fs / kMinF0); }

    float gateDb = -48.0f;      // input level below this is treated as unvoiced
    float threshold = 0.15f;    // YIN absolute threshold

private:
    void analyse() noexcept
    {
        // unwrap ring -> linear (oldest first)
        for (int i = 0; i < bufLen; ++i)
            lin[(size_t) i] = ring[(size_t) ((writeIdx + i) % bufLen)];

        float energy = 0.0f;
        for (int j = 0; j < win; ++j) energy += lin[(size_t) j] * lin[(size_t) j];
        const float rms = std::sqrt (energy / (float) win);
        const float gate = std::pow (10.0f, gateDb / 20.0f);

        bool found = false;
        float bestTau = 0.0f;

        if (rms > gate)
        {
            // difference function
            const float* x = lin.data();
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                float s = 0.0f;
                const float* xt = x + tau;
                for (int j = 0; j < win; ++j)
                {
                    const float d = x[j] - xt[j];
                    s += d * d;
                }
                diff[(size_t) tau] = s;
            }
            // cumulative mean normalised difference
            cmnd[0] = 1.0f;
            float run = 0.0f;
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                run += diff[(size_t) tau];
                cmnd[(size_t) tau] = run > 0.0f ? diff[(size_t) tau] * (float) tau / run : 1.0f;
            }
            int tauEst = -1;
            for (int tau = tauMin; tau < tauMax; ++tau)
            {
                if (cmnd[(size_t) tau] < threshold)
                {
                    while (tau + 1 < tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
                    tauEst = tau;
                    break;
                }
            }
            if (tauEst > 0)
            {
                // parabolic interpolation
                float t = (float) tauEst;
                if (tauEst > 1 && tauEst < tauMax)
                {
                    const float s0 = cmnd[(size_t) tauEst - 1], s1 = cmnd[(size_t) tauEst], s2 = cmnd[(size_t) tauEst + 1];
                    const float den = s0 - 2.0f * s1 + s2;
                    if (std::abs (den) > 1e-9f)
                        t += std::clamp (0.5f * (s0 - s2) / den, -0.5f, 0.5f);
                }
                bestTau = t;
                found = true;
            }
        }

        if (found)
        {
            const float p = bestTau * (float) dec;
            hist[histN % 3] = p;
            ++histN;
            float est = p;
            if (histN >= 3)
            {
                float a = hist[0], b = hist[1], c = hist[2];
                est = std::max (std::min (a, b), std::min (std::max (a, b), c)); // median of 3
                // keep the newest value if it is close to the median (lower lag)
                if (std::abs (p - est) < 0.03f * est) est = p;
            }
            periodSamples = est;
            voicedFlag = true;
            hangover = 2;
        }
        else
        {
            if (hangover > 0) --hangover;
            else { voicedFlag = false; histN = 0; }
        }
    }

    double fs = 48000.0, fsd = 24000.0;
    int dec = 2, tauMin = 24, tauMax = 345, win = 345, bufLen = 700, hop = 96;
    std::vector<float> ring, lin, diff, cmnd;
    int writeIdx = 0, decCount = 0, hopCount = 0, filled = 0;
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float env = 0.0f, envCoef = 0.999f;
    float periodSamples = 240.0f;
    bool voicedFlag = false;
    int hangover = 0;
    float hist[3] {};
    int histN = 0;
};

} // namespace hs
