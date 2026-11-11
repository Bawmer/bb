// OxfordUltraLimiter.h — BUB LEON Oxford Ultra Mastering Limiter
// Combined DSP core: LR4 dual-band split (Cytomic SVF), M/S matrix + sub mono, transient punch,
// 1st-order ADAA "Enhance" waveshaper, Lemire O(1) lookahead with dual-boxcar parabolic ramp,
// AVX2/FMA3 ITU-R BS.1770-4 true-peak interpolator, Wannamaker noise-shaped TPDF dither.
//
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
        inline float dbToLin (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
        inline float linToDb (float lin) noexcept { return 20.0f * std::log10 (std::max (lin, 1.0e-6f)); }

        inline float msToCoeff (float ms, double fs) noexcept
        {
            return 1.0f - std::exp (-1.0f / (std::max (ms, 0.001f) * 0.001f * static_cast<float> (fs)));
        }

        inline int nextPow2 (int v) noexcept { int p = 1; while (p < v) p <<= 1; return p; }

        // ---------------------------------------------------------------------------
        // Cytomic trapezoidal SVF, 4 lanes. LR4 = two cascaded Butterworth (Q = 1/sqrt2) stages.
        // ---------------------------------------------------------------------------
        struct SvfCoeffs
        {
            __m128 a1 {}, a2 {}, a3 {}, k {};
            void set (float fc, float q, double fs) noexcept
            {
                fc = std::clamp (fc, 10.0f, 0.45f * static_cast<float> (fs));
                const float g  = std::tan (std::numbers::pi_v<float> * fc / static_cast<float> (fs));
                const float kk = 1.0f / q;
                const float A1 = 1.0f / (1.0f + g * (g + kk));
                const float A2 = g * A1;
                a1 = _mm_set1_ps (A1); a2 = _mm_set1_ps (A2); a3 = _mm_set1_ps (g * A2); k = _mm_set1_ps (kk);
            }
        };

        struct Svf4
        {
            __m128 ic1 = _mm_setzero_ps(), ic2 = _mm_setzero_ps();
            void reset() noexcept { ic1 = ic2 = _mm_setzero_ps(); }

            // returns lp, writes hp
            inline __m128 tick (__m128 v0, const SvfCoeffs& c, __m128& hp) noexcept
            {
                const __m128 v3 = _mm_sub_ps (v0, ic2);
                const __m128 v1 = _mm_fmadd_ps (c.a1, ic1, _mm_mul_ps (c.a2, v3));
                const __m128 v2 = _mm_add_ps (ic2, _mm_fmadd_ps (c.a2, ic1, _mm_mul_ps (c.a3, v3)));
                ic1 = _mm_sub_ps (_mm_add_ps (v1, v1), ic1);
                ic2 = _mm_sub_ps (_mm_add_ps (v2, v2), ic2);
                hp  = _mm_sub_ps (_mm_sub_ps (v0, _mm_mul_ps (c.k, v1)), v2);
                return v2;
            }
        };

        // Stereo LR4 split: lanes stage1 {L,R,-,-}; stage2 {lpL,lpR,hpL,hpR}
        struct LR4SplitStereo
        {
            SvfCoeffs c; Svf4 s1, s2;
            void reset() noexcept { s1.reset(); s2.reset(); }
            inline void process (float l, float r, float& lfL, float& lfR, float& hfL, float& hfR) noexcept
            {
                __m128 hp1, hp2;
                const __m128 lp1 = s1.tick (_mm_setr_ps (l, r, 0.0f, 0.0f), c, hp1);
                const __m128 lp2 = s2.tick (_mm_movelh_ps (lp1, hp1), c, hp2);
                alignas (16) float lo[4], hi[4];
                _mm_store_ps (lo, lp2); _mm_store_ps (hi, hp2);
                lfL = lo[0]; lfR = lo[1]; hfL = hi[2]; hfR = hi[3];
            }
        };

        // ---------------------------------------------------------------------------
        // ITU-R BS.1770-4 Annex 2 true-peak interpolator: 48-tap Kaiser sinc, 4 phases,
        // pre-transposed into 12 rows. AVX: both channels x 4 phases in one __m256 per tap.
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
                std::fill (std::begin (hL), std::end (hL), 0.0f);
                std::fill (std::begin (hR), std::end (hR), 0.0f);
                w = 0;
            }

            inline void process (float xL, float xR, float& peakL, float& peakR) noexcept
            {
                w = (w == 0) ? kRows - 1 : w - 1;
                hL[w] = hL[w + kRows] = xL;
                hR[w] = hR[w + kRows] = xR;

                __m256 acc = _mm256_setzero_ps();
                for (int j = 0; j < kRows; ++j)
                {
                    const __m256 x = _mm256_set_m128 (_mm_set1_ps (hR[w + j]), _mm_set1_ps (hL[w + j]));
                    acc = _mm256_fmadd_ps (rows[j], x, acc);
                }
                acc = _mm256_andnot_ps (_mm256_set1_ps (-0.0f), acc);
                peakL = std::max (hmax (_mm256_castps256_ps128 (acc)),   std::fabs (hL[w + kLatency]));
                peakR = std::max (hmax (_mm256_extractf128_ps (acc, 1)), std::fabs (hR[w + kLatency]));
            }

        private:
            static inline float hmax (__m128 v) noexcept
            {
                v = _mm_max_ps (v, _mm_movehl_ps (v, v));
                v = _mm_max_ss (v, _mm_shuffle_ps (v, v, 1));
                return _mm_cvtss_f32 (v);
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
                {
                    const __m128 row = _mm_setr_ps ((float) h[4 * j], (float) h[4 * j + 1], (float) h[4 * j + 2], (float) h[4 * j + 3]);
                    rows[j] = _mm256_set_m128 (row, row);
                }
            }

            alignas (32) __m256 rows[kRows] {};
            alignas (32) float hL[2 * kRows] {}, hR[2 * kRows] {};
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
                hist.assign ((size_t) histMask + 1, 1.0f);
                dqMask = nextPow2 (wMax + 2) - 1;
                dqIdx.assign ((size_t) dqMask + 1, 0);
                dqVal.assign ((size_t) dqMask + 1, 1.0f);
                ring1.assign ((size_t) wMax + 1, 1.0f);
                ring2.assign ((size_t) wMax + 1, 1.0f);
                W = std::min (W, wMax);
                reset();
            }

            void reset() noexcept
            {
                std::fill (hist.begin(), hist.end(), 1.0f);
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

            inline float process (float g) noexcept
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
                std::fill (ring1.begin(), ring1.end(), 1.0f);
                std::fill (ring2.begin(), ring2.end(), 1.0f);
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

            inline float chain (float v) noexcept
            {
                const int64_t n = counter++;
                while (dqTail != dqHead && dqVal[(size_t) ((dqTail - 1) & dqMask)] >= v) --dqTail;
                dqIdx[(size_t) (dqTail & dqMask)] = n;
                dqVal[(size_t) (dqTail & dqMask)] = v;
                ++dqTail;
                if (dqIdx[(size_t) (dqHead & dqMask)] <= n - W) ++dqHead;
                const float m = dqVal[(size_t) (dqHead & dqMask)];

                sum1 += (double) m - (double) ring1[(size_t) p1];
                ring1[(size_t) p1] = m; if (++p1 >= B1) p1 = 0;
                const float a1 = (float) (sum1 / B1);

                sum2 += (double) a1 - (double) ring2[(size_t) p2];
                ring2[(size_t) p2] = a1; if (++p2 >= B2) p2 = 0;
                return std::min ((float) (sum2 / B2), 1.0f);
            }

            int wMax = 3, W = 3, B1 = 2, B2 = 2;
            int64_t histPos = 0, histMask = 0, counter = 0;
            int64_t dqHead = 0, dqTail = 0, dqMask = 0;
            int p1 = 0, p2 = 0;
            double sum1 = 2.0, sum2 = 2.0;
            std::vector<float> hist, dqVal, ring1, ring2;
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

            inline float process (float xf, double d, double invTanhD) noexcept
            {
                if (d != dPrev) { lc1 = logCosh (d * x1); dPrev = d; }
                const double x  = xf;
                const double dx = x - x1;
                const double lc = logCosh (d * x);
                double y;
                if (std::fabs (dx) > 1.0e-5) y = (lc - lc1) / (d * dx) * invTanhD;
                else                         y = std::tanh (d * 0.5 * (x + x1)) * invTanhD;   // ill-conditioned fallback
                x1 = x; lc1 = lc;
                return static_cast<float> (y);
            }
        };

        // ---------------------------------------------------------------------------
        // TPDF dither with error-feedback noise shaping (Wannamaker E/F-weighted FIRs).
        // ---------------------------------------------------------------------------
        class ShapedDither
        {
        public:
            void reset() noexcept { e.fill (0.0f); rPrev = 0.0f; }

            void configure (BitDepth bits, NoiseShapingCurve curve) noexcept
            {
                enabled = bits != BitDepth::Float32;
                lsb = bits == BitDepth::Bits16 ? 1.0f / 32768.0f : 1.0f / 8388608.0f;
                static constexpr float eW[5] = { 2.033f, -2.165f, 1.959f, -1.590f, 0.6149f };
                static constexpr float fW[9] = { 2.412f, -3.370f, 3.937f, -4.174f, 3.353f, -2.205f, 1.281f, -0.569f, 0.0847f };
                h.fill (0.0f); order = 0; hpTpdf = false;
                if (curve == NoiseShapingCurve::FlatHighPassTPDF)      { hpTpdf = true; }
                else if (curve == NoiseShapingCurve::ModifiedEWeight)  { std::copy (eW, eW + 5, h.begin()); order = 5; }
                else                                                   { std::copy (fW, fW + 9, h.begin()); order = 9; }
            }

            float getLsb() const noexcept { return enabled ? lsb : 0.0f; }
            bool  isEnabled() const noexcept { return enabled; }

            inline float process (float x, uint32_t& rng) noexcept
            {
                if (! enabled) return x;

                float fb = 0.0f;
                for (int k = 0; k < order; ++k) fb += h[(size_t) k] * e[(size_t) k];
                const float v = x - fb;

                float d;
                if (hpTpdf) { const float r = uniform (rng) - 0.5f; d = (r - rPrev) * lsb; rPrev = r; }
                else        { d = (uniform (rng) - uniform (rng)) * lsb; }

                float q = std::nearbyint ((v + d) / lsb) * lsb;
                q = std::clamp (q, -1.0f, 1.0f - lsb);
                const float err = std::clamp (q - v, -32.0f * lsb, 32.0f * lsb);   // keeps the loop stable on clip
                for (int k = 8; k > 0; --k) e[(size_t) k] = e[(size_t) k - 1];
                e[0] = err;
                return q;
            }

            static inline float uniform (uint32_t& s) noexcept
            {
                s ^= s << 13; s ^= s >> 17; s ^= s << 5;
                return static_cast<float> (s >> 8) * (1.0f / 16777216.0f);
            }

        private:
            std::array<float, 9> h {}, e {};
            int order = 0;
            bool enabled = false, hpTpdf = false;
            float lsb = 1.0f / 8388608.0f, rPrev = 0.0f;
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
        void processOversampledStereo (float& left, float& right) noexcept
        {
            inGain += smoothCoeff * (inGainTarget - inGain);
            ceil   += smoothCoeff * (ceilTarget - ceil);
            if (! smoothSettled)
                glideControls();

            float l = left * inGain, r = right * inGain;

            // ---- 1. LR4 sub-band split ------------------------------------------------
            float lfL, lfR, hfL, hfR;
            split.process (l, r, lfL, lfR, hfL, hfR);

            // ---- 2. LF: elliptical sub mono + independent LF ceiling -------------------
            if (msParams.msModeEnabled)
            {
                const float m = 0.5f * (lfL + lfR);
                const float s = 0.5f * (lfL - lfR) * sideLowKeep;
                lfL = m + s; lfR = m - s;
            }
            const float lfPk = std::max (std::fabs (lfL), std::fabs (lfR));
            lfEnv = lfPk > lfEnv ? lfPk : lfEnv * lfEnvRelease;
            const float lfTarget = std::min (1.0f, ceil * lfHeadroom / std::max (lfEnv, 1.0e-9f));
            gLf += (lfTarget < gLf ? lfAttack : lfRelease) * (lfTarget - gLf);

            // ---- 3. HF: coupling + transient punch ------------------------------------
            const float gHf = 1.0f - coupling * (1.0f - gLf);
            const float hfPk = std::max (std::fabs (hfL), std::fabs (hfR)) + 1.0e-9f;
            fastEnv += (hfPk > fastEnv ? punchFastA : punchFastR) * (hfPk - fastEnv);
            slowEnv += (hfPk > slowEnv ? punchSlowA : punchSlowR) * (hfPk - slowEnv);
            float gPunch = 1.0f;
            if (punchAmount > 0.0f)
            {
                const float ratioDb = std::clamp (detail::linToDb (fastEnv / slowEnv), 0.0f, 6.0f);
                gPunch = detail::dbToLin (punchAmount * 0.5f * ratioDb);    // up to +3 dB on onsets
            }

            float yL = lfL * gLf + hfL * gHf * gPunch;
            float yR = lfR * gLf + hfR * gHf * gPunch;

            // ---- 4. ADAA Enhance (M/S or L/R) + output width --------------------------
            {
                float m = 0.5f * (yL + yR), s = 0.5f * (yL - yR);
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
            float pkL, pkR;
            if (useTpInterpolator)
            {
                float tL, tR;
                tpDetect.process (yL, yR, tL, tR);
                pkL = std::max (tL, prevPkL); pkR = std::max (tR, prevPkR);   // 2-sample hold: covers sub-sample skew
                prevPkL = tL; prevPkR = tR;
            }
            else
            {
                pkL = std::fabs (yL); pkR = std::fabs (yR);
            }

            // ---- 6. Crest-factor auto-release ----------------------------------------
            const float pk = std::max (pkL, pkR);
            crestPeak = pk > crestPeak ? pk : crestPeak * crestPeakRel;
            crestPow += crestRms * (pk * pk - crestPow);
            if (++crestCounter >= 32)
            {
                crestCounter = 0;
                const float crestDb = 10.0f * std::log10 ((crestPeak * crestPeak + 1.0e-12f) / (crestPow + 1.0e-12f));
                const float t = std::clamp ((crestDb - 4.0f) / 10.0f, 0.0f, 1.0f);
                updateReleaseCoeff (1.6f + t * (0.35f - 1.6f));
            }

            // ---- 7. Required gain, link, release envelope, Lemire lookahead ----------
            const float c = ceil;
            float reqL = pkL > c ? c / pkL : 1.0f;
            float reqR = pkR > c ? c / pkR : 1.0f;
            const float linked = std::min (reqL, reqR);
            reqL += link * (linked - reqL);
            reqR += link * (linked - reqR);

            envL = reqL < envL ? reqL : envL + releaseCoeff * (reqL - envL);
            envR = reqR < envR ? reqR : envR + releaseCoeff * (reqR - envR);

            const float sL = gainL.process (envL);
            const float sR = gainR.process (envR);

            // ---- 8. Lookahead delay + gain + safety clamp -----------------------------
            delayL[(size_t) (delayPos & delayMask)] = yL;
            delayR[(size_t) (delayPos & delayMask)] = yR;
            const int64_t rd = delayPos - delayTotal;
            ++delayPos;
            left  = std::clamp (delayL[(size_t) (rd & delayMask)] * sL, -c, c);
            right = std::clamp (delayR[(size_t) (rd & delayMask)] * sR, -c, c);

            // ---- telemetry ------------------------------------------------------------
            const float g = std::min (sL, sR) * guardGain;
            grHold = g < grHold ? g : grHold + grHoldDecay * (g - grHold);
            lastLf = gLf; lastHf = gHf; lastPunch = gPunch;
        }

        // Base-rate output stage: sample-peak safety, output true-peak meter, auto-blanking, dither.
        void processBaseRateDitherStereo (float& left, float& right) noexcept
        {
            // Safety ceiling tracks the limiter's own smoothed ceiling as it emerges from the
            // lookahead delay: rises instantly, falls only after the latency, with the same 20 ms glide.
            if (ceilTarget >= clampCeil)  clampCeil = ceilTarget;
            else if (clampHold > 0)       --clampHold;
            else                          clampCeil += clampSmooth * (ceilTarget - clampCeil);
            if (useGuard)
            {
                float tL, tR;
                guardTp.process (left, right, tL, tR);
                const float pkL = std::max (tL, guardPrevL), pkR = std::max (tR, guardPrevR);
                guardPrevL = tL; guardPrevR = tR;
                const float gc = clampCeil;
                const float req = std::min (pkL > gc ? gc / pkL : 1.0f, pkR > gc ? gc / pkR : 1.0f);  // linked: no image shift
                guardEnvL = req < guardEnvL ? req : guardEnvL + guardRelease * (req - guardEnvL);
                const float g = guardL.process (guardEnvL);

                guardDelayL[(size_t) (guardPos & guardMask)] = left;
                guardDelayR[(size_t) (guardPos & guardMask)] = right;
                const int64_t rd = guardPos - guardLatency;
                ++guardPos;
                left  = guardDelayL[(size_t) (rd & guardMask)] * g;
                right = guardDelayR[(size_t) (rd & guardMask)] * g;
                guardGain = g;
            }

            const float c = clampCeil * 1.0001f;
            left  = std::clamp (left,  -c, c);
            right = std::clamp (right, -c, c);

            float tL, tR;
            tpMeter.process (left, right, tL, tR);
            const float tp = std::max (tL, tR);
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
            t.gainReductionDb   = -detail::linToDb (grHold);
            t.lfGainReductionDb = -detail::linToDb (lastLf);
            t.hfGainReductionDb = -detail::linToDb (lastHf);
            t.punchGainDb       =  detail::linToDb (lastPunch);
            t.outputTruePeakDb  =  tpHold > 1.0e-6f ? detail::linToDb (tpHold) : -120.0f;
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
            const float newCeil = std::max (detail::dbToLin (std::clamp (params.thresholdDb, -30.0f, 0.0f)) - 3.0f * ditherL.getLsb(), 0.01f);
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

            const float amt = std::clamp (params.enhanceAmount, 0.0f, 1.25f);
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
            const float c = smoothCoeff;
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

            const float err = std::max ({ std::fabs (lfHeadroomT - lfHeadroom), std::fabs (couplingT - coupling),
                                          std::fabs (punchAmountT - punchAmount), std::fabs (linkT - link),
                                          std::fabs (sideLowKeepT - sideLowKeep), std::fabs (widthT - width),
                                          (float) std::fabs (driveMidT - driveMid), (float) std::fabs (driveSideT - driveSide) });
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

        void updateReleaseCoeff (float crestFactor) noexcept
        {
            lastReleaseFactor = crestFactor;
            const float ms = std::max (params.releaseMs, 1.0f) * (1.0f + std::clamp (params.autoRelease, 0.0f, 1.0f) * (crestFactor - 1.0f));
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

        std::vector<float> delayL, delayR;

        // base-rate true-peak guard (4x engine)
        bool useGuard = false;
        int  guardW = 0, guardLatency = 0;
        detail::LookaheadGain guardL, guardR;
        std::vector<float> guardDelayL, guardDelayR;
        int64_t guardPos = 0, guardMask = 0;
        float guardEnvL = 1.0f, guardEnvR = 1.0f, guardPrevL = 0.0f, guardPrevR = 0.0f, guardRelease = 0.01f, guardGain = 1.0f;
        int64_t delayPos = 0, delayMask = 0;

        float inGain = 1.0f, inGainTarget = 1.0f, ceil = 0.989f, ceilTarget = 0.989f, smoothCoeff = 0.001f;
        float clampCeil = 0.989f, clampSmooth = 0.001f;
        int   clampHold = 0;
        float lfHeadroom = 0.78f, coupling = 0.15f, punchAmount = 0.2f, link = 0.35f, sideLowKeep = 0.35f, width = 1.0f;
        double driveMid = 1.0, driveSide = 1.0, invTanhMid = 1.0, invTanhSide = 1.0;
        float lfHeadroomT = 0.78f, couplingT = 0.15f, punchAmountT = 0.2f, linkT = 0.35f, sideLowKeepT = 0.35f, widthT = 1.0f;
        double driveMidT = 1.0, driveSideT = 1.0;
        bool smoothSettled = false;

        float envL = 1.0f, envR = 1.0f, releaseCoeff = 0.001f, lastReleaseFactor = 1.0f;
        float lfEnv = 0.0f, lfEnvRelease = 0.999f, gLf = 1.0f, lfAttack = 0.01f, lfRelease = 0.001f;
        float fastEnv = 1.0e-6f, slowEnv = 1.0e-6f, punchFastA = 0, punchFastR = 0, punchSlowA = 0, punchSlowR = 0;
        float crestPeak = 0.0f, crestPow = 1.0e-9f, crestPeakRel = 0.999f, crestRms = 0.001f;
        int   crestCounter = 0;
        float prevPkL = 0.0f, prevPkR = 0.0f;

        float grHold = 1.0f, grHoldDecay = 0.001f, lastLf = 1.0f, lastHf = 1.0f, lastPunch = 1.0f;
        float tpHold = 0.0f, tpHoldDecay = 0.9999f;

        bool autoBlanking = true;
        int silentCount = 0, silenceLimit = 12000;
        uint32_t rngL = 0x9E3779B9u, rngR = 0x85EBCA6Bu;
    };
} // namespace BubLeonDSP
