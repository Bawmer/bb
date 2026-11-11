// OxfordUltraLimiter.h — BUB LEON Oxford Ultra Mastering Limiter
// Combined DSP core: LR4 dual-band split (Cytomic SVF), M/S matrix + sub mono, transient punch,
// 1st-order ADAA "Enhance" waveshaper, Lemire O(1) lookahead with dual-boxcar parabolic ramp,
// AVX2/FMA3 ITU-R BS.1770-4 true-peak interpolator, Wannamaker noise-shaped TPDF dither.
//
// Precision: the whole audio path is 64-bit floating point (AVX2 double SIMD).
// Real-time contract: prepare() is the only allocating call. Everything else is lock-free,
// allocation-free and noexcept.
#pragma once

#include <immintrin.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace BubLeonDSP
{
    enum class BitDepth : int          { Float32 = 0, Bits24 = 1, Bits16 = 2 };
    enum class NoiseShapingCurve : int { FlatHighPassTPDF = 0, ModifiedEWeight = 1, PsychoacousticFWeight = 2 };

    struct LimiterParameters
    {
        float inputGainDb     = 0.0f;
        float thresholdDb     = -0.1f;   // output ceiling (dBTP)
        float attackMs        = 1.5f;    // lookahead ramp length
        float releaseMs       = 120.0f;
        float autoRelease     = 0.8f;    // 0..1 crest-factor adaptive release amount
        float enhanceAmount   = 0.35f;   // 0..1.25 ADAA tanh drive
        float transientPunch  = 0.2f;    // 0..1
        float crossoverHz     = 110.0f;
        float lfHeadroomRatio = 0.78f;   // LF band ceiling as fraction of main ceiling
        float bandCoupling    = 0.15f;   // share of LF gain reduction applied to HF
        BitDepth          ditherBits   = BitDepth::Bits24;
        NoiseShapingCurve ditherShaper = NoiseShapingCurve::PsychoacousticFWeight;
        bool  autoBlanking    = true;
    };

    struct MidSideParameters
    {
        bool  msModeEnabled    = true;
        float midSideLink      = 0.35f;  // L/R detector link for the final stage (0 = independent, 1 = linked)
        float sideLowMono      = 0.65f;  // removes side content below the crossover
        float enhanceSideBoost = 0.30f;  // extra Enhance drive on the side channel
        float stereoWidthOut   = 1.05f;
    };

    struct LimiterTelemetry
    {
        float gainReductionDb   = 0.0f;    // positive dB, peak-held
        float lfGainReductionDb = 0.0f;
        float hfGainReductionDb = 0.0f;
        float punchGainDb       = 0.0f;
        float outputTruePeakDb  = -120.0f; // base-rate output true peak, held
    };

    namespace detail
    {
        // All audio-path math is 64-bit (double). Parameters/telemetry stay float.
        inline float  dbToLin (float db) noexcept   { return std::pow (10.0f, db * 0.05f); }
        inline double dbToLin (double db) noexcept  { return std::pow (10.0, db * 0.05); }
        inline float  linToDb (float lin) noexcept  { return 20.0f * std::log10 (std::max (lin, 1.0e-6f)); }
        inline double linToDb (double lin) noexcept { return 20.0 * std::log10 (std::max (lin, 1.0e-12)); }

        inline double msToCoeff (double ms, double fs) noexcept
        {
            return 1.0 - std::exp (-1.0 / (std::max (ms, 0.001) * 0.001 * fs));
        }

        inline int nextPow2 (int v) noexcept { int p = 1; while (p < v) p <<= 1; return p; }

        // ---------------------------------------------------------------------------
        // Cytomic trapezoidal SVF, 4 lanes x 64-bit (AVX2 __m256d).
        // LR4 = two cascaded Butterworth (Q = 1/sqrt2) stages.
        // ---------------------------------------------------------------------------
        struct SvfCoeffs
        {
            __m256d a1 {}, a2 {}, a3 {}, k {};
            void set (double fc, double q, double fs) noexcept
            {
                fc = std::clamp (fc, 10.0, 0.45 * fs);
                const double g  = std::tan (std::numbers::pi * fc / fs);
                const double kk = 1.0 / q;
                const double A1 = 1.0 / (1.0 + g * (g + kk));
                const double A2 = g * A1;
                a1 = _mm256_set1_pd (A1); a2 = _mm256_set1_pd (A2); a3 = _mm256_set1_pd (g * A2); k = _mm256_set1_pd (kk);
            }
        };

        struct Svf4
        {
            __m256d ic1 = _mm256_setzero_pd(), ic2 = _mm256_setzero_pd();
            void reset() noexcept { ic1 = ic2 = _mm256_setzero_pd(); }

            // returns lp, writes hp
            inline __m256d tick (__m256d v0, const SvfCoeffs& c, __m256d& hp) noexcept
            {
                const __m256d v3 = _mm256_sub_pd (v0, ic2);
                const __m256d v1 = _mm256_fmadd_pd (c.a1, ic1, _mm256_mul_pd (c.a2, v3));
                const __m256d v2 = _mm256_add_pd (ic2, _mm256_fmadd_pd (c.a2, ic1, _mm256_mul_pd (c.a3, v3)));
                ic1 = _mm256_sub_pd (_mm256_add_pd (v1, v1), ic1);
                ic2 = _mm256_sub_pd (_mm256_add_pd (v2, v2), ic2);
                hp  = _mm256_sub_pd (_mm256_sub_pd (v0, _mm256_mul_pd (c.k, v1)), v2);
                return v2;
            }
        };

        // Stereo LR4 split: stage1 lanes {L,R,-,-}; stage2 lanes {lpL,lpR,hpL,hpR}
        struct LR4SplitStereo
        {
            SvfCoeffs c; Svf4 s1, s2;
            void reset() noexcept { s1.reset(); s2.reset(); }
            inline void process (double l, double r, double& lfL, double& lfR, double& hfL, double& hfR) noexcept
            {
                __m256d hp1, hp2;
                const __m256d lp1 = s1.tick (_mm256_setr_pd (l, r, 0.0, 0.0), c, hp1);
                // {lp1L, lp1R, hp1L, hp1R}
                const __m256d in2 = _mm256_permute2f128_pd (lp1, hp1, 0x20);
                const __m256d lp2 = s2.tick (in2, c, hp2);
                alignas (32) double lo[4], hi[4];
                _mm256_store_pd (lo, lp2); _mm256_store_pd (hi, hp2);
                lfL = lo[0]; lfR = lo[1]; hfL = hi[2]; hfR = hi[3];
            }
        };

        // ---------------------------------------------------------------------------
        // ITU-R BS.1770-4 Annex 2 true-peak interpolator: 48-tap Kaiser sinc, 4 phases,
        // pre-transposed into 12 rows of 4 doubles. One __m256d accumulator per channel.
        // Window covered per call: samples (n-6, n-5] at 4x, plus x[n-6]. Latency = 6 samples.
        // ---------------------------------------------------------------------------
        class TruePeakStereo
        {
        public:
            static constexpr int kRows = 12;
            static constexpr int kLatency = 6;

            TruePeakStereo() noexcept { design(); reset(); }

            void reset() noexcept
            {
                std::fill (std::begin (hL), std::end (hL), 0.0);
                std::fill (std::begin (hR), std::end (hR), 0.0);
                w = 0;
            }

            inline void process (double xL, double xR, double& peakL, double& peakR) noexcept
            {
                w = (w == 0) ? kRows - 1 : w - 1;
                hL[w] = hL[w + kRows] = xL;
                hR[w] = hR[w + kRows] = xR;

                __m256d accL = _mm256_setzero_pd(), accR = _mm256_setzero_pd();
                for (int j = 0; j < kRows; ++j)
                {
                    accL = _mm256_fmadd_pd (rows[j], _mm256_set1_pd (hL[w + j]), accL);
                    accR = _mm256_fmadd_pd (rows[j], _mm256_set1_pd (hR[w + j]), accR);
                }
                const __m256d sign = _mm256_set1_pd (-0.0);
                peakL = std::max (hmax (_mm256_andnot_pd (sign, accL)), std::fabs (hL[w + kLatency]));
                peakR = std::max (hmax (_mm256_andnot_pd (sign, accR)), std::fabs (hR[w + kLatency]));
            }

        private:
            static inline double hmax (__m256d v) noexcept
            {
                __m128d m = _mm_max_pd (_mm256_castpd256_pd128 (v), _mm256_extractf128_pd (v, 1));
                m = _mm_max_sd (m, _mm_unpackhi_pd (m, m));
                return _mm_cvtsd_f64 (m);
            }

            static double besselI0 (double x) noexcept
            {
                double sum = 1.0, term = 1.0;
                for (int k = 1; k < 32; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
                return sum;
            }

            void design() noexcept
            {
                constexpr int N = 48; constexpr double beta = 8.0;
                double h[N];
                for (int k = 0; k < N; ++k)
                {
                    const double t = (k - 23.5) / 4.0;
                    const double sinc = std::sin (std::numbers::pi * t) / (std::numbers::pi * t);
                    const double r = 2.0 * k / (N - 1) - 1.0;
                    h[k] = sinc * besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (beta);
                }
                for (int p = 0; p < 4; ++p)            // unity DC gain per phase
                {
                    double s = 0.0; for (int j = 0; j < kRows; ++j) s += h[4 * j + p];
                    for (int j = 0; j < kRows; ++j) h[4 * j + p] /= s;
                }
                for (int j = 0; j < kRows; ++j)
                    rows[j] = _mm256_setr_pd (h[4 * j], h[4 * j + 1], h[4 * j + 2], h[4 * j + 3]);
            }

            alignas (32) __m256d rows[kRows] {};
            alignas (32) double hL[2 * kRows] {}, hR[2 * kRows] {};
            int w = 0;
        };

        // ---------------------------------------------------------------------------
        // Lookahead gain computer: Lemire O(1) sliding minimum over W, then two running-sum
        // boxcars (B1 + B2 - 1 = W). Kernel = trapezoid -> parabolic S-ramp, support W.
        // Guarantee: output gain applied to its aligned sample <= that sample's required gain.
        // A fixed max-window delay keeps the reported latency constant while W changes.
        // ---------------------------------------------------------------------------
        class LookaheadGain
        {
        public:
            void prepare (int maxWindow)
            {
                wMax = std::max (maxWindow, 3);
                histMask = nextPow2 (4 * wMax + 16) - 1;
                hist.assign ((size_t) histMask + 1, 1.0);
                dqMask = nextPow2 (wMax + 2) - 1;
                dqIdx.assign ((size_t) dqMask + 1, 0);
                dqVal.assign ((size_t) dqMask + 1, 1.0);
                ring1.assign ((size_t) wMax + 1, 1.0);
                ring2.assign ((size_t) wMax + 1, 1.0);
                W = std::min (W, wMax);
                reset();
            }

            void reset() noexcept
            {
                std::fill (hist.begin(), hist.end(), 1.0);
                histPos = 0;
                resetChain();
            }

            void setWindow (int newW) noexcept
            {
                newW = std::clamp (newW, 3, wMax);
                if (newW == W) return;
                W = newW;
                rebuild();
            }

            int getMaxWindow() const noexcept { return wMax; }

            inline double process (double g) noexcept
            {
                hist[(size_t) (histPos & histMask)] = g;
                ++histPos;
                return chain (hist[(size_t) ((histPos - 1 - (wMax - W)) & histMask)]);
            }

        private:
            void resetChain() noexcept
            {
                B1 = (W + 1) / 2; B2 = W + 1 - B1;
                dqHead = dqTail = 0; counter = 0;
                std::fill (ring1.begin(), ring1.end(), 1.0);
                std::fill (ring2.begin(), ring2.end(), 1.0);
                p1 = p2 = 0; sum1 = (double) B1; sum2 = (double) B2;
            }

            // Re-stream the last 3W history values so every window is valid immediately.
            void rebuild() noexcept
            {
                resetChain();
                const int64_t n = 3 * (int64_t) W;
                for (int64_t k = n; k >= 1; --k)
                    (void) chain (hist[(size_t) ((histPos - k - (wMax - W)) & histMask)]);
            }

            inline double chain (double v) noexcept
            {
                const int64_t n = counter++;
                while (dqTail != dqHead && dqVal[(size_t) ((dqTail - 1) & dqMask)] >= v) --dqTail;
                dqIdx[(size_t) (dqTail & dqMask)] = n;
                dqVal[(size_t) (dqTail & dqMask)] = v;
                ++dqTail;
                if (dqIdx[(size_t) (dqHead & dqMask)] <= n - W) ++dqHead;
                const double m = dqVal[(size_t) (dqHead & dqMask)];

                sum1 += m - ring1[(size_t) p1];
                ring1[(size_t) p1] = m; if (++p1 >= B1) p1 = 0;
                const double a1 = sum1 / B1;

                sum2 += a1 - ring2[(size_t) p2];
                ring2[(size_t) p2] = a1; if (++p2 >= B2) p2 = 0;
                return std::min (sum2 / B2, 1.0);
            }

            int wMax = 3, W = 3, B1 = 2, B2 = 2;
            int64_t histPos = 0, histMask = 0, counter = 0;
            int64_t dqHead = 0, dqTail = 0, dqMask = 0;
            int p1 = 0, p2 = 0;
            double sum1 = 2.0, sum2 = 2.0;
            std::vector<double> hist, dqVal, ring1, ring2;
            std::vector<int64_t> dqIdx;
        };

        // ---------------------------------------------------------------------------
        // 1st-order ADAA normalised tanh: f(x) = tanh(d x)/tanh(d)
        // F(x) = ln cosh(d x) / (d tanh d), ln cosh z = |z| + log1p(e^-2|z|) - ln 2
        // ---------------------------------------------------------------------------
        struct AdaaTanh
        {
            double x1 = 0.0, lc1 = 0.0, dPrev = -1.0;

            static inline double logCosh (double z) noexcept
            {
                z = std::fabs (z);
                return z + std::log1p (std::exp (-2.0 * z)) - std::numbers::ln2;
            }

            void reset() noexcept { x1 = 0.0; lc1 = 0.0; dPrev = -1.0; }

            inline double process (double x, double d, double invTanhD) noexcept
            {
                if (d != dPrev) { lc1 = logCosh (d * x1); dPrev = d; }
                const double dx = x - x1;
                const double lc = logCosh (d * x);
                double y;
                if (std::fabs (dx) > 1.0e-7) y = (lc - lc1) / (d * dx) * invTanhD;
                else                         y = std::tanh (d * 0.5 * (x + x1)) * invTanhD;   // ill-conditioned fallback
                x1 = x; lc1 = lc;
                return y;
            }
        };

        // ---------------------------------------------------------------------------
        // TPDF dither with error-feedback noise shaping (Wannamaker E/F-weighted FIRs), 64-bit.
        // ---------------------------------------------------------------------------
        class ShapedDither
        {
        public:
            void reset() noexcept { e.fill (0.0); rPrev = 0.0; }

            void configure (BitDepth bits, NoiseShapingCurve curve) noexcept
            {
                enabled = bits != BitDepth::Float32;
                lsb = bits == BitDepth::Bits16 ? 1.0 / 32768.0 : 1.0 / 8388608.0;
                static constexpr double eW[5] = { 2.033, -2.165, 1.959, -1.590, 0.6149 };
                static constexpr double fW[9] = { 2.412, -3.370, 3.937, -4.174, 3.353, -2.205, 1.281, -0.569, 0.0847 };
                h.fill (0.0); order = 0; hpTpdf = false;
                if (curve == NoiseShapingCurve::FlatHighPassTPDF)      { hpTpdf = true; }
                else if (curve == NoiseShapingCurve::ModifiedEWeight)  { std::copy (eW, eW + 5, h.begin()); order = 5; }
                else                                                   { std::copy (fW, fW + 9, h.begin()); order = 9; }
            }

            double getLsb() const noexcept { return enabled ? lsb : 0.0; }
            bool   isEnabled() const noexcept { return enabled; }

            inline double process (double x, uint32_t& rng) noexcept
            {
                if (! enabled) return x;

                double fb = 0.0;
                for (int k = 0; k < order; ++k) fb += h[(size_t) k] * e[(size_t) k];
                const double v = x - fb;

                double d;
                if (hpTpdf) { const double r = uniform (rng) - 0.5; d = (r - rPrev) * lsb; rPrev = r; }
                else        { d = (uniform (rng) - uniform (rng)) * lsb; }

                double q = std::nearbyint ((v + d) / lsb) * lsb;
                q = std::clamp (q, -1.0, 1.0 - lsb);
                const double err = std::clamp (q - v, -32.0 * lsb, 32.0 * lsb);   // keeps the loop stable on clip
                for (int k = 8; k > 0; --k) e[(size_t) k] = e[(size_t) k - 1];
                e[0] = err;
                return q;
            }

            static inline double uniform (uint32_t& s) noexcept
            {
                s ^= s << 13; s ^= s >> 17; s ^= s << 5;
                return static_cast<double> (s) * (1.0 / 4294967296.0);
            }

        private:
            std::array<double, 9> h {}, e {};
            int order = 0;
            bool enabled = false, hpTpdf = false;
            double lsb = 1.0 / 8388608.0, rPrev = 0.0;
        };
    } // namespace detail

    // =====================================================================================
    class OxfordUltraLimiter
    {
    public:
        OxfordUltraLimiter() = default;

        // baseSampleRate = host rate; oversamplingFactor = rate multiplier this instance runs at.
        void prepare (double baseSampleRate, int oversamplingFactor)
        {
            baseFs   = baseSampleRate;
            osFactor = std::max (1, oversamplingFactor);
            fs       = baseFs * osFactor;
            useTpInterpolator = (osFactor == 1);       // at >= 4x the samples already resolve true peak
            dTp = useTpInterpolator ? detail::TruePeakStereo::kLatency : 0;

            // Fixed 10 ms lookahead budget -> constant, integer base-rate latency.
            latencyBase = (int) std::ceil (0.010 * baseFs);

            // 4x engine: base-rate true-peak guard after the downsampler (its ringing can exceed what the
            // limiter saw at 4x). Its latency is taken out of the main lookahead so the total stays 10 ms.
            useGuard = osFactor > 1;
            guardW = useGuard ? std::max (8, (int) std::lround (0.00075 * baseFs)) : 0;
            guardLatency = useGuard ? guardW - 1 + detail::TruePeakStereo::kLatency : 0;
            if (useGuard)
            {
                guardL.prepare (guardW); guardL.setWindow (guardW);
                guardR.prepare (guardW); guardR.setWindow (guardW);
                guardMask = detail::nextPow2 (guardLatency + 2) - 1;
                guardDelayL.assign ((size_t) guardMask + 1, 0.0f);
                guardDelayR.assign ((size_t) guardMask + 1, 0.0f);
                guardRelease = detail::msToCoeff (15.0f, baseFs);
            }

            const int wMax = (latencyBase - guardLatency) * osFactor + 1 - dTp;
            gainL.prepare (wMax);
            gainR.prepare (wMax);
            delayTotal = wMax - 1 + dTp;

            delayMask = detail::nextPow2 (delayTotal + 2) - 1;
            delayL.assign ((size_t) delayMask + 1, 0.0f);
            delayR.assign ((size_t) delayMask + 1, 0.0f);

            smoothCoeff  = detail::msToCoeff (20.0f, fs);
            clampSmooth  = detail::msToCoeff (20.0f, baseFs);
            lfEnvRelease = 1.0f - detail::msToCoeff (60.0f, fs);
            lfAttack     = detail::msToCoeff (1.0f, fs);
            crestPeakRel = 1.0f - detail::msToCoeff (300.0f, fs);
            crestRms     = detail::msToCoeff (300.0f, fs);
            punchFastA   = detail::msToCoeff (0.5f, fs);  punchFastR = detail::msToCoeff (25.0f, fs);
            punchSlowA   = detail::msToCoeff (15.0f, fs); punchSlowR = detail::msToCoeff (150.0f, fs);
            grHoldDecay  = detail::msToCoeff (250.0f, fs);
            tpHoldDecay  = 1.0f - detail::msToCoeff (1500.0f, baseFs);
            silenceLimit = (int) (0.25 * baseFs);

            reset();
            applyParameters();
        }

        void reset() noexcept
        {
            split.reset();
            tpDetect.reset(); tpMeter.reset(); guardTp.reset();
            guardL.reset(); guardR.reset();
            std::fill (guardDelayL.begin(), guardDelayL.end(), 0.0f);
            std::fill (guardDelayR.begin(), guardDelayR.end(), 0.0f);
            guardPos = 0; guardEnvL = guardEnvR = 1.0f; guardPrevL = guardPrevR = 0.0f; guardGain = 1.0f;
            gainL.reset(); gainR.reset();
            adaaA.reset(); adaaB.reset();
            ditherL.reset(); ditherR.reset();
            std::fill (delayL.begin(), delayL.end(), 0.0f);
            std::fill (delayR.begin(), delayR.end(), 0.0f);
            delayPos = 0;
            inGain = inGainTarget; ceil = ceilTarget; clampCeil = ceilTarget; clampHold = 0;
            snapControls();
            envL = envR = 1.0f; lfEnv = 0.0f; gLf = 1.0f;
            fastEnv = slowEnv = 1.0e-6f; crestPeak = 0.0f; crestPow = 1.0e-9f; crestCounter = 0;
            prevPkL = prevPkR = 0.0f;
            grHold = 1.0f; lastLf = lastHf = lastPunch = 1.0f; tpHold = 0.0f;
            silentCount = 0;
            rngL = 0x9E3779B9u; rngR = 0x85EBCA6Bu;
            updateReleaseCoeff (1.0f);
        }

        void setParameters (const LimiterParameters& p) noexcept       { params = p;   applyParameters(); }
        void setMidSideParameters (const MidSideParameters& m) noexcept { msParams = m; applyParameters(); }

        int getInternalLatencyBaseSamples() const noexcept { return latencyBase; }

        // Called once per sample at this instance's internal rate (4x or 1x).
        void processOversampledStereo (double& left, double& right) noexcept
        {
            inGain += smoothCoeff * (inGainTarget - inGain);
            ceil   += smoothCoeff * (ceilTarget - ceil);
            if (! smoothSettled)
                glideControls();

            double l = left * inGain, r = right * inGain;

            // ---- 1. LR4 sub-band split ------------------------------------------------
            double lfL, lfR, hfL, hfR;
            split.process (l, r, lfL, lfR, hfL, hfR);

            // ---- 2. LF: elliptical sub mono + independent LF ceiling -------------------
            if (msParams.msModeEnabled)
            {
                const double m = 0.5f * (lfL + lfR);
                const double s = 0.5f * (lfL - lfR) * sideLowKeep;
                lfL = m + s; lfR = m - s;
            }
            const double lfPk = std::max (std::fabs (lfL), std::fabs (lfR));
            lfEnv = lfPk > lfEnv ? lfPk : lfEnv * lfEnvRelease;
            const double lfTarget = std::min (1.0, ceil * lfHeadroom / std::max (lfEnv, 1.0e-12));
            gLf += (lfTarget < gLf ? lfAttack : lfRelease) * (lfTarget - gLf);

            // ---- 3. HF: coupling + transient punch ------------------------------------
            const double gHf = 1.0f - coupling * (1.0f - gLf);
            const double hfPk = std::max (std::fabs (hfL), std::fabs (hfR)) + 1.0e-9f;
            fastEnv += (hfPk > fastEnv ? punchFastA : punchFastR) * (hfPk - fastEnv);
            slowEnv += (hfPk > slowEnv ? punchSlowA : punchSlowR) * (hfPk - slowEnv);
            double gPunch = 1.0f;
            if (punchAmount > 0.0f)
            {
                const double ratioDb = std::clamp (detail::linToDb (fastEnv / slowEnv), 0.0, 6.0);
                gPunch = detail::dbToLin (punchAmount * 0.5f * ratioDb);    // up to +3 dB on onsets
            }

            double yL = lfL * gLf + hfL * gHf * gPunch;
            double yR = lfR * gLf + hfR * gHf * gPunch;

            // ---- 4. ADAA Enhance (M/S or L/R) + output width --------------------------
            {
                double m = 0.5f * (yL + yR), s = 0.5f * (yL - yR);
                if (msParams.msModeEnabled)
                {
                    m = adaaA.process (m, driveMid,  invTanhMid);
                    s = adaaB.process (s, driveSide, invTanhSide);
                    s *= width;
                    yL = m + s; yR = m - s;
                }
                else
                {
                    yL = adaaA.process (yL, driveMid, invTanhMid);
                    yR = adaaB.process (yR, driveMid, invTanhMid);
                    m = 0.5f * (yL + yR); s = 0.5f * (yL - yR) * width;
                    yL = m + s; yR = m - s;
                }
            }

            // ---- 5. True-peak detection -----------------------------------------------
            double pkL, pkR;
            if (useTpInterpolator)
            {
                double tL, tR;
                tpDetect.process (yL, yR, tL, tR);
                pkL = std::max (tL, prevPkL); pkR = std::max (tR, prevPkR);   // 2-sample hold: covers sub-sample skew
                prevPkL = tL; prevPkR = tR;
            }
            else
            {
                pkL = std::fabs (yL); pkR = std::fabs (yR);
            }

            // ---- 6. Crest-factor auto-release ----------------------------------------
            const double pk = std::max (pkL, pkR);
            crestPeak = pk > crestPeak ? pk : crestPeak * crestPeakRel;
            crestPow += crestRms * (pk * pk - crestPow);
            if (++crestCounter >= 32)
            {
                crestCounter = 0;
                const double crestDb = 10.0f * std::log10 ((crestPeak * crestPeak + 1.0e-12f) / (crestPow + 1.0e-12f));
                const double t = std::clamp ((crestDb - 4.0) / 10.0, 0.0, 1.0);
                updateReleaseCoeff (1.6f + t * (0.35f - 1.6f));
            }

            // ---- 7. Required gain, link, release envelope, Lemire lookahead ----------
            const double c = ceil;
            double reqL = pkL > c ? c / pkL : 1.0f;
            double reqR = pkR > c ? c / pkR : 1.0f;
            const double linked = std::min (reqL, reqR);
            reqL += link * (linked - reqL);
            reqR += link * (linked - reqR);

            envL = reqL < envL ? reqL : envL + releaseCoeff * (reqL - envL);
            envR = reqR < envR ? reqR : envR + releaseCoeff * (reqR - envR);

            const double sL = gainL.process (envL);
            const double sR = gainR.process (envR);

            // ---- 8. Lookahead delay + gain + safety clamp -----------------------------
            delayL[(size_t) (delayPos & delayMask)] = yL;
            delayR[(size_t) (delayPos & delayMask)] = yR;
            const int64_t rd = delayPos - delayTotal;
            ++delayPos;
            left  = std::clamp (delayL[(size_t) (rd & delayMask)] * sL, -c, c);
            right = std::clamp (delayR[(size_t) (rd & delayMask)] * sR, -c, c);

            // ---- telemetry ------------------------------------------------------------
            const double g = std::min (sL, sR) * guardGain;
            grHold = g < grHold ? g : grHold + grHoldDecay * (g - grHold);
            lastLf = gLf; lastHf = gHf; lastPunch = gPunch;
        }

        // Base-rate output stage: sample-peak safety, output true-peak meter, auto-blanking, dither.
        void processBaseRateDitherStereo (double& left, double& right) noexcept
        {
            // Safety ceiling tracks the limiter's own smoothed ceiling as it emerges from the
            // lookahead delay: rises instantly, falls only after the latency, with the same 20 ms glide.
            if (ceilTarget >= clampCeil)  clampCeil = ceilTarget;
            else if (clampHold > 0)       --clampHold;
            else                          clampCeil += clampSmooth * (ceilTarget - clampCeil);
            if (useGuard)
            {
                double tL, tR;
                guardTp.process (left, right, tL, tR);
                const double pkL = std::max (tL, guardPrevL), pkR = std::max (tR, guardPrevR);
                guardPrevL = tL; guardPrevR = tR;
                const double gc = clampCeil;
                const double req = std::min (pkL > gc ? gc / pkL : 1.0f, pkR > gc ? gc / pkR : 1.0f);  // linked: no image shift
                guardEnvL = req < guardEnvL ? req : guardEnvL + guardRelease * (req - guardEnvL);
                const double g = guardL.process (guardEnvL);

                guardDelayL[(size_t) (guardPos & guardMask)] = left;
                guardDelayR[(size_t) (guardPos & guardMask)] = right;
                const int64_t rd = guardPos - guardLatency;
                ++guardPos;
                left  = guardDelayL[(size_t) (rd & guardMask)] * g;
                right = guardDelayR[(size_t) (rd & guardMask)] * g;
                guardGain = g;
            }

            const double c = clampCeil * 1.0001f;
            left  = std::clamp (left,  -c, c);
            right = std::clamp (right, -c, c);

            double tL, tR;
            tpMeter.process (left, right, tL, tR);
            const double tp = std::max (tL, tR);
            tpHold = tp > tpHold ? tp : tpHold * tpHoldDecay;

            if (! ditherL.isEnabled())
                return;

            if (autoBlanking)
            {
                if (std::fabs (left) < 1.0e-8f && std::fabs (right) < 1.0e-8f)
                {
                    if (silentCount < silenceLimit) ++silentCount;
                    else { left = right = 0.0f; ditherL.reset(); ditherR.reset(); return; }
                }
                else silentCount = 0;
            }

            left  = ditherL.process (left,  rngL);
            right = ditherR.process (right, rngR);
        }

        LimiterTelemetry getTelemetry() const noexcept
        {
            LimiterTelemetry t;
            t.gainReductionDb   = (float) -detail::linToDb (grHold);
            t.lfGainReductionDb = (float) -detail::linToDb (lastLf);
            t.hfGainReductionDb = (float) -detail::linToDb (lastHf);
            t.punchGainDb       = (float)  detail::linToDb (lastPunch);
            t.outputTruePeakDb  = tpHold > 1.0e-6 ? (float) detail::linToDb (tpHold) : -120.0f;
            return t;
        }

    private:
        void applyParameters() noexcept
        {
            ditherL.configure (params.ditherBits, params.ditherShaper);
            ditherR.configure (params.ditherBits, params.ditherShaper);
            autoBlanking = params.autoBlanking;

            inGainTarget = detail::dbToLin (std::clamp (params.inputGainDb, -24.0f, 36.0f));
            // -3 LSB pad keeps the requantiser clear of the ceiling
            const double newCeil = std::max ((double) detail::dbToLin (std::clamp (params.thresholdDb, -30.0f, 0.0f)) - 3.0 * ditherL.getLsb(), 0.01);
            if (newCeil < ceilTarget - 1.0e-7f)
                clampHold = latencyBase + 64;              // + oversampler group delay margin
            ceilTarget = newCeil;

            const int W = (int) std::lround (std::clamp (params.attackMs, 0.05f, 10.0f) * 0.001 * fs);
            gainL.setWindow (W);
            gainR.setWindow (W);

            split.c.set (std::clamp (params.crossoverHz, 20.0f, 500.0f), 0.70710678f, fs);
            lfHeadroomT  = std::clamp (params.lfHeadroomRatio, 0.3f, 1.0f);
            couplingT    = std::clamp (params.bandCoupling, 0.0f, 1.0f);
            punchAmountT = std::clamp (params.transientPunch, 0.0f, 1.0f);
            lfRelease   = detail::msToCoeff (std::max (params.releaseMs, 10.0f), fs);
            updateReleaseCoeff (lastReleaseFactor);

            const double amt = std::clamp (params.enhanceAmount, 0.0f, 1.25f);
            driveMidT   = 0.05 + 3.2 * amt;
            driveSideT  = driveMidT * (1.0 + std::clamp (msParams.enhanceSideBoost, 0.0f, 1.0f));

            linkT        = std::clamp (msParams.midSideLink, 0.0f, 1.0f);
            sideLowKeepT = 1.0f - std::clamp (msParams.sideLowMono, 0.0f, 1.0f);
            widthT       = std::clamp (msParams.stereoWidthOut, 0.0f, 2.0f);
            smoothSettled = false;
        }

        // Gain-like controls glide over ~20 ms so automation and preset changes never zipper.
        void glideControls() noexcept
        {
            const double c = smoothCoeff;
            const double cd = c;
            lfHeadroom  += c * (lfHeadroomT - lfHeadroom);
            coupling    += c * (couplingT - coupling);
            punchAmount += c * (punchAmountT - punchAmount);
            link        += c * (linkT - link);
            sideLowKeep += c * (sideLowKeepT - sideLowKeep);
            width       += c * (widthT - width);
            driveMid    += cd * (driveMidT - driveMid);
            driveSide   += cd * (driveSideT - driveSide);
            invTanhMid   = 1.0 / std::tanh (driveMid);
            invTanhSide  = 1.0 / std::tanh (driveSide);

            const double err = std::max ({ std::fabs (lfHeadroomT - lfHeadroom), std::fabs (couplingT - coupling),
                                          std::fabs (punchAmountT - punchAmount), std::fabs (linkT - link),
                                          std::fabs (sideLowKeepT - sideLowKeep), std::fabs (widthT - width),
                                          (double) std::fabs (driveMidT - driveMid), (double) std::fabs (driveSideT - driveSide) });
            if (err < 1.0e-5f) snapControls();
        }

        void snapControls() noexcept
        {
            lfHeadroom = lfHeadroomT; coupling = couplingT; punchAmount = punchAmountT;
            link = linkT; sideLowKeep = sideLowKeepT; width = widthT;
            driveMid = driveMidT; driveSide = driveSideT;
            invTanhMid = 1.0 / std::tanh (driveMid);
            invTanhSide = 1.0 / std::tanh (driveSide);
            smoothSettled = true;
        }

        void updateReleaseCoeff (double crestFactor) noexcept
        {
            lastReleaseFactor = crestFactor;
            const double ms = std::max (params.releaseMs, 1.0f) * (1.0f + std::clamp (params.autoRelease, 0.0f, 1.0f) * (crestFactor - 1.0f));
            releaseCoeff = detail::msToCoeff (ms, fs);
        }

        LimiterParameters params {};
        MidSideParameters msParams {};

        double baseFs = 48000.0, fs = 192000.0;
        int osFactor = 4, dTp = 0, latencyBase = 0, delayTotal = 0;
        bool useTpInterpolator = false;

        detail::LR4SplitStereo split;
        detail::TruePeakStereo tpDetect, tpMeter, guardTp;
        detail::LookaheadGain  gainL, gainR;
        detail::AdaaTanh       adaaA, adaaB;
        detail::ShapedDither   ditherL, ditherR;

        std::vector<double> delayL, delayR;

        // base-rate true-peak guard (4x engine)
        bool useGuard = false;
        int  guardW = 0, guardLatency = 0;
        detail::LookaheadGain guardL, guardR;
        std::vector<double> guardDelayL, guardDelayR;
        int64_t guardPos = 0, guardMask = 0;
        double guardEnvL = 1.0f, guardEnvR = 1.0f, guardPrevL = 0.0f, guardPrevR = 0.0f, guardRelease = 0.01f, guardGain = 1.0f;
        int64_t delayPos = 0, delayMask = 0;

        double inGain = 1.0f, inGainTarget = 1.0f, ceil = 0.989f, ceilTarget = 0.989f, smoothCoeff = 0.001f;
        double clampCeil = 0.989f, clampSmooth = 0.001f;
        int   clampHold = 0;
        double lfHeadroom = 0.78f, coupling = 0.15f, punchAmount = 0.2f, link = 0.35f, sideLowKeep = 0.35f, width = 1.0f;
        double driveMid = 1.0, driveSide = 1.0, invTanhMid = 1.0, invTanhSide = 1.0;
        double lfHeadroomT = 0.78f, couplingT = 0.15f, punchAmountT = 0.2f, linkT = 0.35f, sideLowKeepT = 0.35f, widthT = 1.0f;
        double driveMidT = 1.0, driveSideT = 1.0;
        bool smoothSettled = false;

        double envL = 1.0f, envR = 1.0f, releaseCoeff = 0.001f, lastReleaseFactor = 1.0f;
        double lfEnv = 0.0f, lfEnvRelease = 0.999f, gLf = 1.0f, lfAttack = 0.01f, lfRelease = 0.001f;
        double fastEnv = 1.0e-6f, slowEnv = 1.0e-6f, punchFastA = 0, punchFastR = 0, punchSlowA = 0, punchSlowR = 0;
        double crestPeak = 0.0f, crestPow = 1.0e-9f, crestPeakRel = 0.999f, crestRms = 0.001f;
        int   crestCounter = 0;
        double prevPkL = 0.0f, prevPkR = 0.0f;

        double grHold = 1.0f, grHoldDecay = 0.001f, lastLf = 1.0f, lastHf = 1.0f, lastPunch = 1.0f;
        double tpHold = 0.0f, tpHoldDecay = 0.9999f;

        bool autoBlanking = true;
        int silentCount = 0, silenceLimit = 12000;
        uint32_t rngL = 0x9E3779B9u, rngR = 0x85EBCA6Bu;
    };
} // namespace BubLeonDSP
