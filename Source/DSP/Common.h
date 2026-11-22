#pragma once
// Bub Leon Mix Rack — shared DSP building blocks (double precision, real-time safe)
#include <cmath>
#include <algorithm>
#include <array>
#include <vector>
#include <cstdint>

namespace bl
{
constexpr double kPi = 3.14159265358979323846;

inline double dbToGain (double db) noexcept   { return std::pow (10.0, db * 0.05); }
inline double gainToDb (double g) noexcept    { return 20.0 * std::log10 (std::max (g, 1.0e-12)); }
inline double clampd (double v, double lo, double hi) noexcept { return std::min (hi, std::max (lo, v)); }

/** One-pole coefficient for a time constant in seconds. */
inline double timeCoef (double seconds, double fs) noexcept
{
    if (seconds <= 0.0 || fs <= 0.0) return 0.0;
    return std::exp (-1.0 / (seconds * fs));
}

//==============================================================================
/** Stereo transposed-direct-form-II biquad (RBJ cookbook designs). */
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] {}, z2[2] {};

    void reset() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0.0; }
    void identity() noexcept { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    inline double process (double x, int c) noexcept
    {
        const double y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return y;
    }

    static double safeFreq (double f, double fs) noexcept { return clampd (f, 5.0, fs * 0.47); }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
    {
        const double inv = 1.0 / na0;
        b0 = nb0 * inv; b1 = nb1 * inv; b2 = nb2 * inv; a1 = na1 * inv; a2 = na2 * inv;
    }

    void peak (double fs, double f, double q, double db) noexcept
    {
        if (std::abs (db) < 1.0e-6) { identity(); return; }
        const double A = std::pow (10.0, db / 40.0), w = 2 * kPi * safeFreq (f, fs) / fs;
        const double alpha = std::sin (w) / (2 * std::max (q, 0.05)), cw = std::cos (w);
        set (1 + alpha * A, -2 * cw, 1 - alpha * A, 1 + alpha / A, -2 * cw, 1 - alpha / A);
    }

    void lowShelf (double fs, double f, double q, double db) noexcept
    {
        if (std::abs (db) < 1.0e-6) { identity(); return; }
        const double A = std::pow (10.0, db / 40.0), w = 2 * kPi * safeFreq (f, fs) / fs;
        const double cw = std::cos (w), alpha = std::sin (w) / (2 * std::max (q, 0.05)), sA = 2 * std::sqrt (A) * alpha;
        set (A * ((A + 1) - (A - 1) * cw + sA), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - sA),
             (A + 1) + (A - 1) * cw + sA, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sA);
    }

    void highShelf (double fs, double f, double q, double db) noexcept
    {
        if (std::abs (db) < 1.0e-6) { identity(); return; }
        const double A = std::pow (10.0, db / 40.0), w = 2 * kPi * safeFreq (f, fs) / fs;
        const double cw = std::cos (w), alpha = std::sin (w) / (2 * std::max (q, 0.05)), sA = 2 * std::sqrt (A) * alpha;
        set (A * ((A + 1) + (A - 1) * cw + sA), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - sA),
             (A + 1) - (A - 1) * cw + sA, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sA);
    }

    void highPass (double fs, double f, double q) noexcept
    {
        const double w = 2 * kPi * safeFreq (f, fs) / fs, cw = std::cos (w), alpha = std::sin (w) / (2 * q);
        set ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
    }

    void lowPass (double fs, double f, double q) noexcept
    {
        const double w = 2 * kPi * safeFreq (f, fs) / fs, cw = std::cos (w), alpha = std::sin (w) / (2 * q);
        set ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
    }

    void bandPass (double fs, double f, double q) noexcept
    {
        const double w = 2 * kPi * safeFreq (f, fs) / fs, cw = std::cos (w), alpha = std::sin (w) / (2 * q);
        set (alpha, 0, -alpha, 1 + alpha, -2 * cw, 1 - alpha);
    }
};

//==============================================================================
/** First-order DC blocker. */
struct DCBlocker
{
    double R = 0.9995, x1[2] {}, y1[2] {};
    void prepare (double fs) noexcept { R = 1.0 - 2.0 * kPi * 8.0 / fs; reset(); }
    void reset() noexcept { x1[0] = x1[1] = y1[0] = y1[1] = 0; }
    inline double process (double x, int c) noexcept
    {
        const double y = x - x1[c] + R * y1[c];
        x1[c] = x; y1[c] = y;
        return y;
    }
};

//==============================================================================
/** Linear one-pole parameter smoother (block-rate target, sample-rate glide). */
struct Smoother
{
    double cur = 0, target = 0, coef = 0.0;
    void prepare (double fs, double seconds = 0.02) noexcept { coef = timeCoef (seconds, fs); }
    void snap (double v) noexcept { cur = target = v; }
    void set (double v) noexcept { target = v; }
    inline double next() noexcept { cur = target + (cur - target) * coef; return cur; }
    bool isSmoothing() const noexcept { return std::abs (cur - target) > 1.0e-9; }
};

//==============================================================================
/** Polynomial soft clipper (rational tanh approximation, smooth to |x| = 3). */
inline double softClip (double x) noexcept
{
    if (x > 3.0) return 1.0;
    if (x < -3.0) return -1.0;
    const double x2 = x * x;
    return x * (27.0 + x2) / (27.0 + 9.0 * x2);
}

/** Small deterministic white-noise generator (xorshift). */
struct Noise
{
    uint32_t s = 0x1234567u;
    inline double next() noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (double) s * (2.0 / 4294967295.0) - 1.0;
    }
};

//==============================================================================
/** Fixed-capacity stereo delay line with cubic interpolation. */
struct DelayLine
{
    std::vector<double> buf[2];
    int size = 1, w = 0;

    void prepare (int capacity)
    {
        size = 1;
        while (size < capacity + 4) size <<= 1;
        buf[0].assign ((size_t) size, 0.0);
        buf[1].assign ((size_t) size, 0.0);
        w = 0;
    }
    void reset() noexcept { std::fill (buf[0].begin(), buf[0].end(), 0.0); std::fill (buf[1].begin(), buf[1].end(), 0.0); w = 0; }
    inline void push (double l, double r) noexcept
    {
        buf[0][(size_t) w] = l; buf[1][(size_t) w] = r;
        w = (w + 1) & (size - 1);
    }
    /** Integer-delay read (delay >= 1 means the previously pushed sample is delay 1). */
    inline double read (int c, int delay) const noexcept
    {
        return buf[c][(size_t) ((w - delay) & (size - 1))];
    }
    inline double readFrac (int c, double delay) const noexcept
    {
        const int d = (int) delay;
        const double t = delay - d;
        const double y0 = read (c, d - 1 < 1 ? 1 : d - 1), y1 = read (c, std::max (1, d)),
                     y2 = read (c, d + 1), y3 = read (c, d + 2);
        const double c0 = y1, c1 = 0.5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3,
                     c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }
};
} // namespace bl
