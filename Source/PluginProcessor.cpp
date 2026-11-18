#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using hs::kNumStylePresets;
float dbToGain (float db, float floorDb = -59.9f) { return db <= floorDb ? 0.0f : juce::Decibels::decibelsToGain (db); }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout HarmonyStackProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto fmt = [] (const char* unit, int dec, bool sign = false)
    {
        return AudioParameterFloatAttributes().withLabel (unit)
            .withStringFromValueFunction ([unit, dec, sign] (float v, int)
                { return (sign && v > 0.0f ? "+" : "") + String (v, dec) + " " + unit; });
    };
    auto pct = fmt ("%", 0);
    auto panAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int)
            { const int a = (int) std::lround (std::abs (v)); return a == 0 ? String ("C") : (v < 0 ? "L" : "R") + String (a); })
        .withValueFromStringFunction ([] (const String& t)
            { auto u = t.trim().toUpperCase(); if (u == "C") return 0.0f;
              const float n = u.retainCharacters ("0123456789.").getFloatValue(); return u.startsWith ("L") ? -n : n; });
    auto db  = AudioParameterFloatAttributes().withLabel ("dB")
                   .withStringFromValueFunction ([] (float v, int) { return v <= -59.9f ? String ("Off") : String (v, 1) + " dB"; });

    StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    StringArray scales;
    for (int i = 0; i < hs::kNumScales; ++i) scales.add (hs::scaleTable()[i].name);

    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ID::mode, 1 }, "Mode", StringArray { "Scale", "Chromatic", "MIDI" }, 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ID::key, 1 }, "Key", keys, 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ID::scale, 1 }, "Scale", scales, 0));

    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ID::humanize, 1 }, "Humanize", NormalisableRange<float> (0, 100, 0.1f), 25.0f, pct));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ID::dbl, 1 }, "Double", StringArray { "Off", "On" }, 0));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ID::dblWidth, 1 }, "Double Width", NormalisableRange<float> (0, 100, 0.1f), 60.0f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ID::formant, 1 }, "Formant", NormalisableRange<float> (-6, 6, 0.01f), 0.0f, fmt ("st", 2, true)));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ID::breath, 1 }, "Breath", NormalisableRange<float> (0, 100, 0.1f), 25.0f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ID::glide, 1 }, "Glide", NormalisableRange<float> (0, 300, 0.1f, 0.5f), 40.0f, fmt ("ms", 0)));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ID::vibDepth, 1 }, "Vibrato", NormalisableRange<float> (0, 100, 0.1f), 0.0f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ID::vibRate, 1 }, "Vibrato Rate", NormalisableRange<float> (2, 9, 0.01f), 5.5f, fmt ("Hz", 2)));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ID::gate, 1 }, "Tracking Gate", NormalisableRange<float> (-80, -20, 0.1f), -50.0f, fmt ("dB", 1)));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ID::dry, 1 }, "Dry", NormalisableRange<float> (-60, 6, 0.1f, 2.0f), 0.0f, db));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ID::harmony, 1 }, "Harmony", NormalisableRange<float> (-60, 6, 0.1f, 2.0f), -3.0f, db));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ID::output, 1 }, "Output", NormalisableRange<float> (-24, 12, 0.1f), 0.0f, fmt ("dB", 1, true)));

    const int   defInt[4] = { 2, 4, -2, -7 };
    const bool  defOn[4]  = { true, false, false, false };
    const float defPan[4] = { -25, 25, -60, 60 };
    for (int v = 0; v < 4; ++v)
    {
        const String n = "Voice " + String (v + 1) + " ";
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ID::vOn (v), 1 }, n + "On", StringArray { "Off", "On" }, defOn[v] ? 1 : 0));
        ps.push_back (std::make_unique<AudioParameterInt>   (ParameterID { ID::vInt (v), 1 }, n + "Interval", -12, 12, defInt[v]));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ID::vLevel (v), 1 }, n + "Level", NormalisableRange<float> (-60, 6, 0.1f, 2.0f), 0.0f, db));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ID::vPan (v), 1 }, n + "Pan", NormalisableRange<float> (-100, 100, 0.1f), defPan[v], panAttr));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ID::vDetune (v), 1 }, n + "Detune", NormalisableRange<float> (-50, 50, 0.1f), 0.0f, fmt ("ct", 1, true)));
    }
    return { ps.begin(), ps.end() };
}

HarmonyStackProcessor::HarmonyStackProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BubLeonHarmonyStack", createLayout())
{
    for (auto& a : uiVoiceMidi) a.store (-1.0f);
    formats.registerBasicFormats();
    auto g = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    raw.mode = g (ID::mode); raw.key = g (ID::key); raw.scale = g (ID::scale);
    raw.humanize = g (ID::humanize); raw.dbl = g (ID::dbl); raw.dblWidth = g (ID::dblWidth);
    raw.formant = g (ID::formant); raw.breath = g (ID::breath); raw.glide = g (ID::glide);
    raw.vibDepth = g (ID::vibDepth); raw.vibRate = g (ID::vibRate); raw.gate = g (ID::gate);
    raw.dry = g (ID::dry); raw.harmony = g (ID::harmony); raw.output = g (ID::output);
    for (int v = 0; v < 4; ++v)
    {
        raw.on[v] = g (ID::vOn (v)); raw.interval[v] = g (ID::vInt (v)); raw.level[v] = g (ID::vLevel (v));
        raw.pan[v] = g (ID::vPan (v)); raw.detune[v] = g (ID::vDetune (v));
    }
}

bool HarmonyStackProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void HarmonyStackProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    chunk = juce::jmax (64, samplesPerBlock);
    engine.prepare (sampleRate, chunk);
    setLatencySamples (engine.latencySamples());

    monoBuf.setSize (1, chunk);
    wetBuf.setSize (2, chunk);
    dryLen = engine.latencySamples();
    dryDelay.setSize (2, dryLen + 1);
    dryDelay.clear();
    dryWrite = 0;

    for (auto* s : { &dryGain, &harmGain, &outGain }) s->reset (sampleRate, 0.03);
    pullParameters();
    dryGain.setCurrentAndTargetValue (dbToGain (raw.dry->load()));
    harmGain.setCurrentAndTargetValue (dbToGain (raw.harmony->load()));
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (raw.output->load()));
}

hs::EngineSnapshot HarmonyStackProcessor::makeSnapshot()
{
    hs::EngineSnapshot sn;
    auto& g = sn.g;
    g.mode = (hs::Mode) juce::jlimit (0, 2, (int) raw.mode->load());
    g.key = (int) raw.key->load();
    g.scale = (int) raw.scale->load();
    g.humanize = raw.humanize->load() * 0.01f;
    g.doubleOn = raw.dbl->load() > 0.5f;
    g.doubleWidth = raw.dblWidth->load() * 0.01f;
    g.formantSemis = raw.formant->load();
    g.breath = raw.breath->load() * 0.01f;
    g.glideMs = raw.glide->load();
    g.vibratoDepth = raw.vibDepth->load() * 0.01f;
    g.vibratoRate = raw.vibRate->load();
    sn.gateDb = raw.gate->load();
    sn.dryGain = dbToGain (raw.dry->load());
    sn.harmGain = dbToGain (raw.harmony->load());
    for (int v = 0; v < 4; ++v)
    {
        auto& vp = sn.v[v];
        vp.enabled = raw.on[v]->load() > 0.5f;
        vp.interval = (int) std::lround (raw.interval[v]->load());
        vp.gain = dbToGain (raw.level[v]->load());
        vp.pan = raw.pan[v]->load() * 0.01f;
        vp.detuneCents = raw.detune[v]->load();
    }
    return sn;
}

void HarmonyStackProcessor::pullParameters()
{
    const auto sn = makeSnapshot();
    engine.setGlobal (sn.g);
    engine.setGateDb (sn.gateDb);
    for (int v = 0; v < 4; ++v) engine.setVoice (v, sn.v[v]);
}

void HarmonyStackProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())                         engine.noteOn (m.getNoteNumber());
        else if (m.isNoteOff())                   engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }
    uiHeldNotes.store (engine.heldNoteCount());

    pullParameters();
    dryGain.setTargetValue (dbToGain (raw.dry->load()));
    harmGain.setTargetValue (dbToGain (raw.harmony->load()));
    outGain.setTargetValue (juce::Decibels::decibelsToGain (raw.output->load()));

    float peak = 0.0f;
    for (int start = 0; start < n; start += chunk)
    {
        const int len = juce::jmin (chunk, n - start);
        float* mono = monoBuf.getWritePointer (0);
        if (numIn >= 2)
        {
            const float* l = buffer.getReadPointer (0, start);
            const float* r = buffer.getReadPointer (1, start);
            for (int i = 0; i < len; ++i) mono[i] = 0.5f * (l[i] + r[i]);
        }
        else if (numIn == 1)
            juce::FloatVectorOperations::copy (mono, buffer.getReadPointer (0, start), len);
        else
            juce::FloatVectorOperations::clear (mono, len);

        // capture the incoming vocal for offline generation (follows host transport)
        if (captureArmed.load())
        {
            bool playing = true;
            if (auto* ph = getPlayHead())
                if (auto pos = ph->getPosition())
                    playing = pos->getIsPlaying();
            int cl = captureLen.load();
            if (playing && cl < captureBuf.getNumSamples())
            {
                const int m = juce::jmin (len, captureBuf.getNumSamples() - cl);
                juce::FloatVectorOperations::copy (captureBuf.getWritePointer (0, cl), mono, m);
                captureLen.store (cl + m);
                captureActive.store (true);
            }
            else if (captureActive.load() || cl >= captureBuf.getNumSamples())
            {
                captureArmed.store (false);
                captureActive.store (false);
                captureDone.store (true);
            }
        }

        wetBuf.clear (0, len);
        engine.process (mono, wetBuf.getWritePointer (0), wetBuf.getWritePointer (1), len);

        float* wl = wetBuf.getWritePointer (0);
        float* wr = wetBuf.getWritePointer (1);
        float* dl = dryDelay.getWritePointer (0);
        float* dr = dryDelay.getWritePointer (1);
        float* outL = buffer.getWritePointer (0, start);
        float* outR = numOut > 1 ? buffer.getWritePointer (1, start) : nullptr;
        const float* inR = numIn > 1 ? buffer.getReadPointer (1, start) : outL;
        const int ringLen = dryLen + 1;

        for (int i = 0; i < len; ++i)
        {
            // latency-compensated dry path (keeps the lead in phase with the stack)
            const float xl = outL[i], xr = inR[i];
            dl[dryWrite] = xl; dr[dryWrite] = xr;
            int rd = dryWrite - dryLen; if (rd < 0) rd += ringLen;
            const float yl = dl[rd], yr = dr[rd];
            if (++dryWrite >= ringLen) dryWrite = 0;

            const float dg = dryGain.getNextValue(), hg = harmGain.getNextValue(), og = outGain.getNextValue();
            float L = (yl * dg + wl[i] * hg) * og;
            float R = (yr * dg + wr[i] * hg) * og;
            if (outR == nullptr) { L = 0.5f * (L + R); }
            else outR[i] = R;
            outL[i] = L;
            peak = juce::jmax (peak, std::abs (L), std::abs (R));
        }
    }
    // audition of the last generated Lead + Stack
    if (previewPlaying.load())
    {
        const juce::SpinLock::ScopedTryLockType tl (previewLock);
        if (tl.isLocked() && previewBuf.getNumSamples() > 0)
        {
            const int pos = previewPos.load();
            const int m = juce::jmin (n, previewBuf.getNumSamples() - pos);
            for (int ch = 0; ch < juce::jmin (numOut, 2); ++ch)
                buffer.addFrom (ch, 0, previewBuf, numOut == 1 ? 0 : ch, pos, m, numOut == 1 ? 0.5f : 1.0f);
            if (numOut == 1 && m > 0) buffer.addFrom (0, 0, previewBuf, 1, pos, m, 0.5f);
            previewPos.store (pos + m);
            if (pos + m >= previewBuf.getNumSamples()) previewPlaying.store (false);
        }
    }

    for (int ch = numOut; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);

    uiInputMidi.store (engine.detectedMidi());
    for (int v = 0; v < 4; ++v)
        uiVoiceMidi[(size_t) v].store (raw.on[v]->load() > 0.5f && engine.detectedMidi() > 0.0f ? engine.voiceTargetMidi (v) : -1.0f);
    uiOutLevel.store (juce::jmax (peak, uiOutLevel.load() * 0.9f));
}

//==============================================================================
int HarmonyStackProcessor::getNumPrograms() { return kNumStylePresets; }

const juce::String HarmonyStackProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, kNumStylePresets) ? juce::String (hs::stylePresets()[index].name) : juce::String();
}

void HarmonyStackProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, kNumStylePresets)) return;
    currentProgram = index;
    const auto& pr = hs::stylePresets()[index];
    auto set = [this] (const juce::String& id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    };
    set (ID::mode, (float) pr.mode);
    set (ID::humanize, pr.humanize); set (ID::dbl, pr.dbl); set (ID::dblWidth, pr.width);
    set (ID::formant, pr.formant); set (ID::breath, pr.breath); set (ID::glide, pr.glide);
    set (ID::vibDepth, pr.vibDepth); set (ID::vibRate, pr.vibRate);
    set (ID::dry, pr.dryDb); set (ID::harmony, pr.harmDb);
    for (int v = 0; v < 4; ++v)
    {
        set (ID::vOn (v), pr.v[v].on ? 1.0f : 0.0f);
        set (ID::vInt (v), (float) pr.v[v].interval);
        set (ID::vLevel (v), pr.v[v].levelDb);
        set (ID::vPan (v), pr.v[v].pan * 100.0f);
        set (ID::vDetune (v), pr.v[v].detune);
    }
}

void HarmonyStackProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("autoKey", autoKey, nullptr);
    state.setProperty ("allStyles", allStyles, nullptr);
    state.setProperty ("voiceStems", voiceStems, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void HarmonyStackProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) vt.getProperty ("program", 0);
            autoKey = (bool) vt.getProperty ("autoKey", true);
            allStyles = (bool) vt.getProperty ("allStyles", false);
            voiceStems = (bool) vt.getProperty ("voiceStems", true);
            apvts.replaceState (vt);
        }
}

//==============================================================================
void HarmonyStackProcessor::setStem (std::shared_ptr<juce::AudioBuffer<float>> b, double sr, const juce::String& name)
{
    stem = std::move (b);
    stemSampleRate = sr;
    stemName = name;
    stemOverview.assign (600, 0.0f);
    const int n = stem->getNumSamples();
    for (int i = 0; i < 600; ++i)
    {
        const int a = (int) ((int64_t) n * i / 600), e = (int) ((int64_t) n * (i + 1) / 600);
        float pk = 0.0f;
        for (int c = 0; c < stem->getNumChannels(); ++c)
            if (e > a) pk = juce::jmax (pk, stem->getMagnitude (c, a, e - a));
        stemOverview[(size_t) i] = pk;
    }
    ++stemSerial;
}

bool HarmonyStackProcessor::loadStem (const juce::File& file, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) { error = "Unsupported audio file: " + file.getFileName(); return false; }
    const double maxSeconds = 20.0 * 60.0;
    if (reader->lengthInSamples <= 0 || reader->lengthInSamples > (juce::int64) (maxSeconds * reader->sampleRate))
    { error = "File must be between 0 and 20 minutes long."; return false; }
    const int chans = (int) juce::jlimit (1u, 2u, reader->numChannels);
    auto b = std::make_shared<juce::AudioBuffer<float>> (chans, (int) reader->lengthInSamples);
    reader->read (b.get(), 0, (int) reader->lengthInSamples, 0, true, chans > 1);
    setStem (b, reader->sampleRate, file.getFileNameWithoutExtension());
    return true;
}

void HarmonyStackProcessor::armCapture (bool shouldArm)
{
    if (shouldArm)
    {
        if (captureArmed.load()) return;
        const double sr = getSampleRate() > 0 ? getSampleRate() : 48000.0;
        captureBuf.setSize (1, (int) (sr * 60.0 * 12.0), false, false, true);   // up to 12 minutes
        captureLen.store (0);
        captureDone.store (false);
        captureActive.store (false);
        captureArmed.store (true);
    }
    else if (captureArmed.exchange (false))
    {
        captureActive.store (false);
        captureDone.store (captureLen.load() > 0);
    }
}

void HarmonyStackProcessor::pollBackground()
{
    if (captureDone.exchange (false))
    {
        const int len = captureLen.load();
        const double sr = getSampleRate() > 0 ? getSampleRate() : 48000.0;
        if (len > (int) (0.5 * sr))
        {
            auto b = std::make_shared<juce::AudioBuffer<float>> (1, len);
            b->copyFrom (0, 0, captureBuf, 0, 0, len);
            setStem (b, sr, "Captured Take " + juce::Time::getCurrentTime().formatted ("%H-%M-%S"));
        }
        captureBuf.setSize (1, 0);
    }

    if (renderer.hasResult())
    {
        lastResult = renderer.takeResult();
        ++resultSerial;
        if (lastResult.key >= 0)
        {
            auto set = [this] (const juce::String& id, float value)
            { if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (value)); };
            set (ID::key, (float) lastResult.key);
            set (ID::scale, lastResult.minor ? 1.0f : 0.0f);
        }
        if (lastResult.preview != nullptr)
        {
            // resample the preview to the host rate if needed (message thread)
            juce::AudioBuffer<float> pv;
            const double hostSr = getSampleRate() > 0 ? getSampleRate() : lastResult.sampleRate;
            const double ratio = lastResult.sampleRate / hostSr;
            if (std::abs (ratio - 1.0) < 1.0e-6) pv.makeCopyOf (*lastResult.preview);
            else
            {
                const int outLen = (int) (lastResult.preview->getNumSamples() / ratio);
                pv.setSize (2, outLen);
                for (int c = 0; c < 2; ++c)
                {
                    juce::LagrangeInterpolator li;
                    li.process (ratio, lastResult.preview->getReadPointer (c), pv.getWritePointer (c), outLen);
                }
            }
            previewPlaying.store (false);
            const juce::SpinLock::ScopedLockType sl (previewLock);
            std::swap (previewBuf, pv);
            previewLen = previewBuf.getNumSamples();
            previewPos.store (0);
        }
    }
}

bool HarmonyStackProcessor::startGenerate (juce::String& error)
{
    if (stem == nullptr) { error = "Load or capture a vocal stem first."; return false; }
    if (renderer.isBusy()) { error = "Already rendering."; return false; }
    HarmonyRenderer::Job job;
    job.stem = stem;
    job.sampleRate = stemSampleRate;
    job.stemName = stemName;
    job.snap = makeSnapshot();
    job.styleName = getProgramName (currentProgram);
    job.autoKey = autoKey && job.snap.g.mode == hs::Mode::Scale;
    job.allStyles = allStyles;
    job.voiceStems = voiceStems;
    job.baseDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Bub Leon Harmony Stack");
    job.baseDir.createDirectory();
    if (! renderer.start (std::move (job))) { error = "Could not start render."; return false; }
    return true;
}

void HarmonyStackProcessor::setPreviewPlaying (bool shouldPlay)
{
    if (shouldPlay && previewLen > 0)
    {
        if (previewPos.load() >= previewLen) previewPos.store (0);
        previewPlaying.store (true);
    }
    else
    {
        previewPlaying.store (false);
        if (! shouldPlay) previewPos.store (0);
    }
}

juce::AudioProcessorEditor* HarmonyStackProcessor::createEditor() { return new HarmonyStackEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HarmonyStackProcessor(); }
