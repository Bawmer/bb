// Bub Leon Harmony Stack — background job: analyse stem, detect key, render harmony files for drag & drop
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "dsp/OfflineRender.h"
#include "dsp/KeyDetector.h"
#include "Labels.h"
#include <memory>
#include <atomic>

class HarmonyRenderer : private juce::Thread
{
public:
    struct Job
    {
        std::shared_ptr<const juce::AudioBuffer<float>> stem;
        double sampleRate = 44100.0;
        juce::String stemName { "Vocal" };
        hs::EngineSnapshot snap;
        juce::String styleName { "Custom" };
        bool autoKey = true;
        bool allStyles = false;
        bool voiceStems = true;
        juce::File baseDir;
    };

    struct OutputFile { juce::String label; juce::File file; };

    struct Result
    {
        bool ok = false;
        juce::String message;
        std::vector<OutputFile> files;
        juce::File folder;
        int key = -1;
        bool minor = false;
        float keyConfidence = 0.0f;
        std::shared_ptr<juce::AudioBuffer<float>> preview;   // Lead + Stack of the (first) style
        double sampleRate = 44100.0;
    };

    HarmonyRenderer() : juce::Thread ("BubLeonHarmonyRender") {}
    ~HarmonyRenderer() override { stopThread (5000); }

    bool start (Job j)
    {
        if (isThreadRunning()) return false;
        job = std::move (j);
        progress = 0.0f;
        resultReady = false;
        setStatus ("Analysing vocal...");
        return startThread();
    }
    void cancel()              { signalThreadShouldExit(); }
    bool isBusy() const        { return isThreadRunning(); }
    bool hasResult() const     { return resultReady.load(); }
    Result takeResult()        { const juce::ScopedLock sl (lock); resultReady = false; return std::move (result); }
    juce::String getStatus()   { const juce::ScopedLock sl (lock); return status; }

    std::atomic<float> progress { 0.0f };

private:
    void setStatus (const juce::String& s) { const juce::ScopedLock sl (lock); status = s; }

    static bool writeWav (const juce::File& f, double sr, const float* l, const float* r, int64_t n, float gain)
    {
        f.deleteFile();
        auto os = f.createOutputStream();
        if (os == nullptr) return false;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
        if (w == nullptr) return false;
        os.release();   // writer owns the stream now
        const int block = 8192;
        juce::AudioBuffer<float> tmp (2, block);
        for (int64_t pos = 0; pos < n; pos += block)
        {
            const int len = (int) std::min<int64_t> (block, n - pos);
            for (int i = 0; i < len; ++i)
            {
                tmp.setSample (0, i, l[pos + i] * gain);
                tmp.setSample (1, i, r[pos + i] * gain);
            }
            if (! w->writeFromAudioSampleBuffer (tmp, 0, len)) return false;
        }
        return true;
    }

    static float peakOf (const std::vector<float>& a, const std::vector<float>& b)
    {
        float p = 0.0f;
        for (size_t i = 0; i < a.size(); ++i) p = std::max ({ p, std::abs (a[i]), std::abs (b[i]) });
        return p;
    }

    static juce::String safeName (juce::String s)
    {
        s = s.replaceCharacters ("\\/:*?\"<>|", "---------").trim();
        return s.isEmpty() ? juce::String ("Vocal") : s.substring (0, 60);
    }

    void run() override
    {
        Result res;
        res.sampleRate = job.sampleRate;
        const auto& stem = *job.stem;
        const int64_t n = stem.getNumSamples();
        const int chans = stem.getNumChannels();

        std::vector<float> mono ((size_t) n);
        for (int64_t i = 0; i < n; ++i)
        {
            float s = 0.0f;
            for (int c = 0; c < chans; ++c) s += stem.getSample (c, (int) i);
            mono[(size_t) i] = s / (float) juce::jmax (1, chans);
        }

        hs::EngineSnapshot base = job.snap;
        if (base.g.mode == hs::Mode::Midi && ! job.allStyles)
        {
            res.message = "MIDI mode follows live MIDI notes - pick a Scale or Chromatic style to auto-generate.";
            finish (std::move (res));
            return;
        }

        // ---- 1. key detection ----
        if (job.autoKey)
        {
            setStatus ("Detecting key...");
            const auto k = hs::detectKey (mono.data(), n, job.sampleRate);
            if (k.voicedSeconds > 1.5f)
            {
                res.key = k.key; res.minor = k.minor; res.keyConfidence = k.confidence;
                base.g.key = k.key;
                base.g.scale = k.minor ? 1 : 0;   // Natural Minor / Major
            }
        }
        progress = 0.08f;
        if (threadShouldExit()) { res.message = "Cancelled"; finish (std::move (res)); return; }

        // ---- 2. output folder ----
        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
        res.folder = job.baseDir.getChildFile (safeName (job.stemName) + " - " + (job.allStyles ? juce::String ("All Styles") : safeName (job.styleName)) + " - " + stamp);
        if (! res.folder.createDirectory())
        {
            res.message = "Could not create output folder: " + res.folder.getFullPathName();
            finish (std::move (res));
            return;
        }

        // ---- 3. render ----
        std::vector<std::pair<juce::String, hs::EngineSnapshot>> styles;
        if (job.allStyles)
        {
            for (int i = 0; i < hs::kNumStylePresets; ++i)
            {
                if (hs::stylePresets()[i].mode == 2) continue;   // MIDI style needs live notes
                auto s = base;
                hs::applyStyle (hs::stylePresets()[i], s);
                styles.push_back ({ hs::stylePresets()[i].name, s });
            }
        }
        else styles.push_back ({ job.styleName, base });

        const auto stemTag = safeName (job.stemName);
        for (size_t si = 0; si < styles.size(); ++si)
        {
            const auto& [name, snap] = styles[si];
            setStatus ("Rendering " + name + "...");
            const float p0 = 0.08f + 0.9f * (float) si / (float) styles.size();
            const float pw = 0.9f / (float) styles.size();

            hs::RenderOutput out;
            const bool stems = job.voiceStems && ! job.allStyles;
            const bool ok = hs::renderHarmony (mono.data(), n, job.sampleRate, snap, out, stems,
                                               [this, p0, pw] (float f) { progress = p0 + pw * f; return ! threadShouldExit(); });
            if (! ok) { res.message = "Cancelled"; finish (std::move (res)); return; }

            // one safety gain for the stack and its stems so they still sum correctly
            const float pk = peakOf (out.stackL, out.stackR);
            const float g = pk > 0.98f ? 0.98f / pk : 1.0f;
            const auto styleTag = safeName (name);

            auto stackFile = res.folder.getChildFile (job.allStyles ? stemTag + " - " + styleTag + ".wav"
                                                                    : stemTag + " - Harmony Stack (" + styleTag + ").wav");
            if (! writeWav (stackFile, job.sampleRate, out.stackL.data(), out.stackR.data(), out.length, g))
            { res.message = "Could not write " + stackFile.getFileName(); finish (std::move (res)); return; }
            res.files.push_back ({ job.allStyles ? name : juce::String ("Harmony Stack"), stackFile });

            // Lead + Stack mix (and preview)
            if (si == 0 || ! job.allStyles)
            {
                std::vector<float> ml ((size_t) out.length), mr ((size_t) out.length);
                const float dry = snap.dryGain > 0.0f ? snap.dryGain : 1.0f;
                for (int64_t i = 0; i < out.length; ++i)
                {
                    const float ll = i < n ? stem.getSample (0, (int) i) : 0.0f;
                    const float rr = i < n ? stem.getSample (chans > 1 ? 1 : 0, (int) i) : 0.0f;
                    ml[(size_t) i] = ll * dry + out.stackL[(size_t) i] * snap.harmGain;
                    mr[(size_t) i] = rr * dry + out.stackR[(size_t) i] * snap.harmGain;
                }
                const float pm = peakOf (ml, mr);
                const float gm = pm > 0.98f ? 0.98f / pm : 1.0f;
                if (! job.allStyles)
                {
                    auto mixFile = res.folder.getChildFile (stemTag + " - Lead + Stack (" + styleTag + ").wav");
                    if (writeWav (mixFile, job.sampleRate, ml.data(), mr.data(), out.length, gm))
                        res.files.push_back ({ "Lead + Stack", mixFile });
                }
                auto pv = std::make_shared<juce::AudioBuffer<float>> (2, (int) out.length);
                for (int64_t i = 0; i < out.length; ++i)
                {
                    pv->setSample (0, (int) i, ml[(size_t) i] * gm);
                    pv->setSample (1, (int) i, mr[(size_t) i] * gm);
                }
                res.preview = pv;
            }

            if (stems)
                for (int v = 0; v < 4; ++v)
                {
                    if (! snap.v[v].enabled) continue;
                    const auto lbl = "Voice " + juce::String (v + 1) + " - "
                                   + labels::intervalLabel ((int) snap.g.mode, snap.g.scale, snap.v[v].interval, v);
                    auto vf = res.folder.getChildFile (stemTag + " - " + safeName (lbl) + ".wav");
                    if (writeWav (vf, job.sampleRate, out.voiceL[v].data(), out.voiceR[v].data(), out.length, g))
                        res.files.push_back ({ "Voice " + juce::String (v + 1), vf });
                }
        }

        res.ok = true;
        res.message = juce::String ((int) res.files.size()) + " files ready - drag them into your DAW";
        progress = 1.0f;
        finish (std::move (res));
    }

    void finish (Result r)
    {
        const juce::ScopedLock sl (lock);
        status = r.message;
        result = std::move (r);
        resultReady = true;
    }

    Job job;
    Result result;
    juce::String status;
    juce::CriticalSection lock;
    std::atomic<bool> resultReady { false };
};
