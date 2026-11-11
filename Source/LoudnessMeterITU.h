// LoudnessMeterITU.h — BUB LEON Oxford Ultra Mastering Limiter
// ITU-R BS.1770-4 / EBU R128 loudness: K-weighting (any sample rate), momentary (400 ms),
// short-term (3 s), gated integrated (abs -70 LUFS, rel -10 LU), PLR, and 1:1 loudness match.
//
// Audio thread: prepare() allocates, processSampleStereo() is RT-safe.
// GUI thread:   getTelemetry() reads relaxed atomics only.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <numbers>
#include <vector>

namespace BubLeonDSP
{
    struct LoudnessTelemetry
    {
        float inputShortTermLufs   = -70.0f;
        float outputShortTermLufs  = -70.0f;
        float outputMomentaryLufs  = -70.0f;
        float outputIntegratedLufs = -70.0f;
        float truePeakDb           = -120.0f;   // maximum since reset
        float plrDb                = 0.0f;
        float matchGainDb          = 0.0f;
        bool  matchEnabled         = false;
    };

    namespace loudness_detail
    {
        struct Biquad
        {
            double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
            inline double process (double x) noexcept
            {
                const double y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }
            void reset() noexcept { z1 = z2 = 0; }
        };

        // BS.1770 K-weighting re-derived for arbitrary fs (matches the 48 kHz reference coefficients).
        struct KWeighting
        {
            Biquad shelf, hpf;

            void prepare (double fs) noexcept
            {
                {
                    const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
                    const double K = std::tan (std::numbers::pi * f0 / fs);
                    const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
                    const double a0 = 1.0 + K / Q + K * K;
                    shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
                    shelf.b1 = 2.0 * (K * K - Vh) / a0;
                    shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
                    shelf.a1 = 2.0 * (K * K - 1.0) / a0;
                    shelf.a2 = (1.0 - K / Q + K * K) / a0;
                }
                {
                    const double f0 = 38.13547087602444, Q = 0.5003270373238773;
                    const double K = std::tan (std::numbers::pi * f0 / fs);
                    const double a0 = 1.0 + K / Q + K * K;
                    hpf.b0 = 1.0; hpf.b1 = -2.0; hpf.b2 = 1.0;
                    hpf.a1 = 2.0 * (K * K - 1.0) / a0;
                    hpf.a2 = (1.0 - K / Q + K * K) / a0;
                }
                reset();
            }

            void reset() noexcept { shelf.reset(); hpf.reset(); }
            inline double process (double x) noexcept { return hpf.process (shelf.process (x)); }
        };

        inline float energyToLufs (double e) noexcept
        {
            return e > 1.0e-10 ? std::max (-70.0f, (float) (-0.691 + 10.0 * std::log10 (e))) : -70.0f;
        }

        // 100 ms sub-block engine for one stereo signal.
        struct StereoLoudness
        {
            KWeighting kL, kR;
            std::array<double, 30> blocks {};   // 30 x 100 ms = 3 s
            int blockWrite = 0, blocksFilled = 0;
            double acc = 0.0;

            void prepare (double fs) noexcept { kL.prepare (fs); kR.prepare (fs); reset(); }
            void reset() noexcept { kL.reset(); kR.reset(); blocks.fill (0.0); blockWrite = blocksFilled = 0; acc = 0.0; }

            inline void push (float l, float r) noexcept
            {
                const double a = kL.process (l), b = kR.process (r);
                acc += a * a + b * b;
            }

            // Close a 100 ms sub-block (n samples). Returns energy of that sub-block.
            void closeBlock (int n) noexcept
            {
                blocks[(size_t) blockWrite] = acc / (double) n;
                acc = 0.0;
                blockWrite = (blockWrite + 1) % 30;
                blocksFilled = std::min (blocksFilled + 1, 30);
            }

            double meanOfLast (int count) const noexcept
            {
                count = std::min (count, blocksFilled);
                if (count == 0) return 0.0;
                double s = 0.0;
                for (int i = 1; i <= count; ++i) s += blocks[(size_t) ((blockWrite - i + 30) % 30)];
                return s / (double) count;
            }
        };
    } // namespace loudness_detail

    class LoudnessAnalyzerITU
    {
    public:
        // maxLatencySamples: largest plugin latency the dry-path delay must cover (allocated here).
        void prepare (double sampleRate, int maxLatencySamples)
        {
            fs = sampleRate;
            inMeter.prepare (fs);
            outMeter.prepare (fs);
            blockLen = std::max (1, (int) std::lround (0.1 * fs));
            delayCapacity = std::max (0, maxLatencySamples);
            delayLen = delayCapacity;
            dryL.assign ((size_t) delayCapacity + 1, 0.0f);
            dryR.assign ((size_t) delayCapacity + 1, 0.0f);
            gainSmooth = 1.0f - std::exp (-1.0f / (0.05f * (float) fs));
            reset();
        }

        void reset() noexcept
        {
            inMeter.reset(); outMeter.reset();
            std::fill (dryL.begin(), dryL.end(), 0.0f);
            std::fill (dryR.begin(), dryR.end(), 0.0f);
            dryPos = 0; blockCount = 0;
            histCount.fill (0); histEnergy.fill (0.0);
            matchDb = 0.0f; matchLin = 1.0f; matchLinTarget = 1.0f; tpMax = 0.0f;
            publish (-70.0f, -70.0f, -70.0f, -70.0f);
        }

        void setAutoGainMatchEnabled (bool shouldMatch) noexcept { matchOn = shouldMatch; }

        // RT-safe: re-aligns the dry path after an engine/latency change (no allocation).
        void setLatencySamples (int latencySamples) noexcept
        {
            const int newLen = std::clamp (latencySamples, 0, delayCapacity);
            if (newLen == delayLen) return;
            delayLen = newLen;
            std::fill (dryL.begin(), dryL.end(), 0.0f);
            std::fill (dryR.begin(), dryR.end(), 0.0f);
            dryPos = 0;
        }

        // RT-safe: restart the gated integrated measurement and the true-peak maximum.
        void resetIntegrated() noexcept
        {
            histCount.fill (0);
            histEnergy.fill (0.0);
            tpMax = 0.0f;
        }

        // inL/inR: raw (pre-limiter) input. outL/outR: limiter output, gain-matched in place when enabled.
        void processSampleStereo (float inL, float inR, float& outL, float& outR, float outTruePeakLin) noexcept
        {
            float dl = inL, dr = inR;
            if (delayLen > 0)
            {
                dl = dryL[(size_t) dryPos]; dr = dryR[(size_t) dryPos];
                dryL[(size_t) dryPos] = inL; dryR[(size_t) dryPos] = inR;
                if (++dryPos > delayLen - 1) dryPos = 0;
            }

            inMeter.push (dl, dr);
            outMeter.push (outL, outR);               // measured pre-match: true limiter output loudness
            tpMax = std::max (tpMax, outTruePeakLin);          // max true peak since reset (EBU Tech 3342)

            matchLin += gainSmooth * ((matchOn ? matchLinTarget : 1.0f) - matchLin);
            outL *= matchLin; outR *= matchLin;

            if (++blockCount >= blockLen)
            {
                blockCount = 0;
                closeBlock();
            }
        }

        LoudnessTelemetry getTelemetry() const noexcept
        {
            LoudnessTelemetry t;
            t.inputShortTermLufs   = aInST.load (std::memory_order_relaxed);
            t.outputShortTermLufs  = aOutST.load (std::memory_order_relaxed);
            t.outputMomentaryLufs  = aOutM.load (std::memory_order_relaxed);
            t.outputIntegratedLufs = aOutI.load (std::memory_order_relaxed);
            t.truePeakDb           = aTp.load (std::memory_order_relaxed);
            t.plrDb                = aPlr.load (std::memory_order_relaxed);
            t.matchGainDb          = aMatch.load (std::memory_order_relaxed);
            t.matchEnabled         = aMatchOn.load (std::memory_order_relaxed);
            return t;
        }

    private:
        static constexpr int kHistBins = 800;   // 0.1 LU bins, -70 .. +10 LUFS

        void closeBlock() noexcept
        {
            inMeter.closeBlock (blockLen);
            outMeter.closeBlock (blockLen);

            const float inST  = loudness_detail::energyToLufs (inMeter.meanOfLast (30));
            const double mE   = outMeter.meanOfLast (4);
            const float outM  = loudness_detail::energyToLufs (mE);
            const float outST = loudness_detail::energyToLufs (outMeter.meanOfLast (30));

            // Gated integrated loudness from 400 ms blocks at 75 % overlap (100 ms hop)
            if (outMeter.blocksFilled >= 4 && outM > -70.0f)
            {
                const int bin = std::clamp ((int) ((outM + 70.0f) * 10.0f), 0, kHistBins - 1);
                ++histCount[(size_t) bin];
                histEnergy[(size_t) bin] += mE;
            }
            float outI = -70.0f;
            {
                double e = 0.0; long long n = 0;
                for (int b = 0; b < kHistBins; ++b) { e += histEnergy[(size_t) b]; n += histCount[(size_t) b]; }
                if (n > 0)
                {
                    const float relGate = loudness_detail::energyToLufs (e / (double) n) - 10.0f;
                    const int startBin = std::clamp ((int) std::ceil ((relGate + 70.0f) * 10.0f), 0, kHistBins);
                    double e2 = 0.0; long long n2 = 0;
                    for (int b = startBin; b < kHistBins; ++b) { e2 += histEnergy[(size_t) b]; n2 += histCount[(size_t) b]; }
                    if (n2 > 0) outI = loudness_detail::energyToLufs (e2 / (double) n2);
                }
            }

            // 1:1 match: bring output short-term loudness down to the input's (never boost)
            if (inST > -60.0f && outST > -60.0f)
            {
                const float target = std::clamp (inST - outST, -24.0f, 0.0f);
                matchDb += 0.15f * (target - matchDb);
                matchLinTarget = std::pow (10.0f, matchDb * 0.05f);
            }

            publish (inST, outST, outM, outI);
        }

        void publish (float inST, float outST, float outM, float outI) noexcept
        {
            const float tpDb = tpMax > 1.0e-6f ? 20.0f * std::log10 (tpMax) : -120.0f;
            aInST.store (inST, std::memory_order_relaxed);
            aOutST.store (outST, std::memory_order_relaxed);
            aOutM.store (outM, std::memory_order_relaxed);
            aOutI.store (outI, std::memory_order_relaxed);
            aTp.store (tpDb, std::memory_order_relaxed);
            // PLR = max true peak - integrated loudness (short-term until integrated is available)
            const float ref = outI > -70.0f ? outI : outST;
            aPlr.store (ref > -70.0f ? tpDb - ref : 0.0f, std::memory_order_relaxed);
            aMatch.store (matchOn ? matchDb : 0.0f, std::memory_order_relaxed);
            aMatchOn.store (matchOn, std::memory_order_relaxed);
        }

        double fs = 48000.0;
        loudness_detail::StereoLoudness inMeter, outMeter;
        int blockLen = 4800, blockCount = 0;

        std::vector<float> dryL, dryR;
        int delayLen = 0, delayCapacity = 0, dryPos = 0;

        std::array<int, kHistBins>    histCount {};
        std::array<double, kHistBins> histEnergy {};

        bool  matchOn = false;
        float matchDb = 0.0f, matchLin = 1.0f, matchLinTarget = 1.0f, gainSmooth = 0.001f;
        float tpMax = 0.0f;

        std::atomic<float> aInST { -70.0f }, aOutST { -70.0f }, aOutM { -70.0f }, aOutI { -70.0f };
        std::atomic<float> aTp { -120.0f }, aPlr { 0.0f }, aMatch { 0.0f };
        std::atomic<bool>  aMatchOn { false };
    };
} // namespace BubLeonDSP
