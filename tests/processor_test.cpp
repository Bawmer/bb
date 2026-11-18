// End-to-end test of the plugin processor: stem load, auto key, render to WAV, all-styles batch, capture, preview.
#include "../Source/PluginProcessor.h"
#include <cstdio>

static int fails = 0;
static void ok (bool c, const char* what, const juce::String& extra = {})
{
    if (! c) ++fails;
    std::printf ("%-46s %s %s\n", what, c ? "OK" : "FAIL", extra.toRawUTF8());
}

static juce::AudioBuffer<float> sungMelody (double sr, std::vector<int> notes, double noteLen)
{
    const int per = (int) (noteLen * sr);
    juce::AudioBuffer<float> b (1, per * (int) notes.size());
    double ph = 0;
    for (size_t k = 0; k < notes.size(); ++k)
    {
        const double f = 440.0 * std::pow (2.0, (notes[k] - 69) / 12.0);
        for (int i = 0; i < per; ++i)
        {
            ph += f / sr; ph -= std::floor (ph);
            double s = 0;
            for (int h = 1; h * f < sr * 0.4; ++h) s += std::sin (2 * juce::MathConstants<double>::pi * h * ph) / std::pow (h, 1.2);
            const double env = std::min (1.0, std::min (i / (0.02 * sr), (per - i) / (0.02 * sr)));
            b.setSample (0, (int) k * per + i, (float) (0.2 * s * env));
        }
    }
    return b;
}

static bool waitForResult (HarmonyStackProcessor& p, int serialBefore)
{
    for (int i = 0; i < 1200 && p.resultSerial == serialBefore; ++i)
    {
        juce::Thread::sleep (50);
        p.pollBackground();
    }
    return p.resultSerial != serialBefore;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("hs_test");
    tmp.deleteRecursively();
    tmp.createDirectory();

    // write a stem: melody in A minor
    auto stem = sungMelody (sr, { 57, 60, 64, 62, 60, 59, 57, 64, 65, 64, 62, 60, 59, 57 }, 0.4);
    auto stemFile = tmp.getChildFile ("Lead Vocal.wav");
    {
        juce::WavAudioFormat wav;
        auto os = stemFile.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 1, 24, {}, 0));
        os.release();
        w->writeFromAudioSampleBuffer (stem, 0, stem.getNumSamples());
    }

    HarmonyStackProcessor p;
    p.setPlayConfigDetails (2, 2, sr, 512);
    p.prepareToPlay (sr, 512);

    juce::String err;
    ok (p.loadStem (stemFile, err), "Load stem WAV", err);
    ok (p.stemName == "Lead Vocal" && p.stem->getNumSamples() == stem.getNumSamples(), "Stem decoded correctly");

    // set the session to the wrong key (D major) to prove auto-key overrides it
    p.apvts.getParameter (ID::key)->setValueNotifyingHost (p.apvts.getParameter (ID::key)->convertTo0to1 (2));
    p.setCurrentProgram (1);   // Pop Stack 3rd + 5th
    p.autoKey = true; p.allStyles = false; p.voiceStems = true;

    int before = p.resultSerial;
    ok (p.startGenerate (err), "Start generate", err);
    ok (waitForResult (p, before), "Render finished");
    auto& r = p.lastResult;
    ok (r.ok, "Render ok", r.message);
    ok (r.key == 9 && r.minor, "Auto key = A minor", juce::String (labels::noteName (r.key)) + (r.minor ? " minor" : " major"));
    ok ((int) p.apvts.getRawParameterValue (ID::key)->load() == 9 && (int) p.apvts.getRawParameterValue (ID::scale)->load() == 1,
        "Key/Scale parameters updated");
    // Stack + Lead+Stack + 2 voice stems
    ok (r.files.size() == 4, "4 files (stack, lead+stack, 2 voices)", juce::String ((int) r.files.size()));

    juce::AudioFormatManager fm; fm.registerBasicFormats();
    for (auto& f : r.files)
    {
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f.file));
        const bool exists = rd != nullptr;
        float lo = 0, hi = 0, lo2 = 0, hi2 = 0;
        if (exists) rd->readMaxLevels (0, rd->lengthInSamples, lo, hi, lo2, hi2);
        const bool lenOk = exists && rd->lengthInSamples >= stem.getNumSamples() && rd->sampleRate == sr && rd->bitsPerSample == 24 && rd->numChannels == 2;
        ok (lenOk && hi > 0.02f && hi <= 0.99f, ("  " + f.file.getFileName()).toRawUTF8(),
            exists ? "peak " + juce::String (hi, 2) : juce::String ("missing"));
    }
    ok (r.preview != nullptr, "Preview buffer created");

    if (juce::SystemStats::getEnvironmentVariable ("HS_SNAPSHOT", {}).isNotEmpty())
    {
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
        for (int pos = 0; pos < 12 * 512; pos += 512) { b.copyFrom (0, 0, stem, 0, 4000 + pos, 512); b.copyFrom (1, 0, stem, 0, 4000 + pos, 512); p.processBlock (b, m); }
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditorIfNeeded());
        ed->setVisible (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        juce::File out (juce::SystemStats::getEnvironmentVariable ("HS_SNAPSHOT", {}));
        out.deleteFile();
        juce::FileOutputStream fos (out);
        juce::PNGImageFormat().writeImageToStream (img, fos);
        ed.reset();
    }

    // preview playback through processBlock
    {
        p.setPreviewPlaying (true);
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
        float mx = 0;
        for (int i = 0; i < 200; ++i) { b.clear(); p.processBlock (b, m); mx = juce::jmax (mx, b.getMagnitude (0, 512)); }
        ok (mx > 0.01f, "Preview audible in plugin output", juce::String (mx, 3));
        p.setPreviewPlaying (false);
    }

    // all-styles batch
    p.allStyles = true;
    before = p.resultSerial;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    ok (p.startGenerate (err), "Start ALL STYLES", err);
    ok (waitForResult (p, before), "ALL STYLES finished", juce::String ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 2) + " s");
    ok (p.lastResult.ok && p.lastResult.files.size() == (size_t) hs::kNumStylePresets - 1, "One stack per style (16)", juce::String ((int) p.lastResult.files.size()));
    p.allStyles = false;

    // capture (standalone: no playhead -> records while armed)
    {
        p.armCapture (true);
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
        for (int pos = 0; pos + 512 <= stem.getNumSamples(); pos += 512)
        {
            b.copyFrom (0, 0, stem, 0, pos, 512);
            b.copyFrom (1, 0, stem, 0, pos, 512);
            p.processBlock (b, m);
        }
        ok (p.isCapturing(), "Capturing while armed");
        p.armCapture (false);
        p.pollBackground();
        ok (p.stem != nullptr && p.stemName.startsWith ("Captured") && std::abs (p.stem->getNumSamples() - stem.getNumSamples()) < 600,
            "Captured take became the stem", juce::String (p.stem->getNumSamples()));
        before = p.resultSerial;
        p.startGenerate (err);
        waitForResult (p, before);
        ok (p.lastResult.ok && p.lastResult.key == 9, "Generate from captured take", p.lastResult.message);
    }

    // MIDI style can't be auto-rendered -> clear message
    p.setCurrentProgram (hs::kMidiStyleIndex);
    before = p.resultSerial;
    p.startGenerate (err); waitForResult (p, before);
    ok (! p.lastResult.ok && p.lastResult.message.contains ("MIDI"), "MIDI style gives a helpful message");

    std::printf (fails ? "\n%d FAILURES\n" : "\nALL PROCESSOR CHECKS PASSED\n", fails);
    return fails ? 1 : 0;
}
