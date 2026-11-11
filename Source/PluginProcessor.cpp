#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

OxfordUltraAudioProcessor::OxfordUltraAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "OxfordUltraState", createParameterLayout())
{
    using namespace BubLeonDSP::ParamIDs;
    paramPtrs.inputGain        = apvts.getRawParameterValue(inputGain);
    paramPtrs.threshold        = apvts.getRawParameterValue(threshold);
    paramPtrs.attack           = apvts.getRawParameterValue(attack);
    paramPtrs.release          = apvts.getRawParameterValue(release);
    paramPtrs.autoRelease      = apvts.getRawParameterValue(autoRelease);
    paramPtrs.enhance          = apvts.getRawParameterValue(enhance);
    paramPtrs.transientPunch   = apvts.getRawParameterValue(transientPunch);
    paramPtrs.crossoverHz      = apvts.getRawParameterValue(crossoverHz);
    paramPtrs.lfHeadroom       = apvts.getRawParameterValue(lfHeadroom);
    paramPtrs.bandCoupling     = apvts.getRawParameterValue(bandCoupling);
    paramPtrs.msMode           = apvts.getRawParameterValue(msMode);
    paramPtrs.midSideLink      = apvts.getRawParameterValue(midSideLink);
    paramPtrs.sideLowMono      = apvts.getRawParameterValue(sideLowMono);
    paramPtrs.enhanceSideBoost = apvts.getRawParameterValue(enhanceSideBoost);
    paramPtrs.stereoWidth      = apvts.getRawParameterValue(stereoWidth);
    paramPtrs.ditherBits       = apvts.getRawParameterValue(ditherBits);
    paramPtrs.ditherShaper     = apvts.getRawParameterValue(ditherShaper);
    paramPtrs.autoBlanking     = apvts.getRawParameterValue(autoBlanking);
    paramPtrs.autoGainMatch    = apvts.getRawParameterValue(autoGainMatch);
    paramPtrs.oversampling     = apvts.getRawParameterValue(oversampling);
}

juce::AudioProcessorValueTreeState::ParameterLayout OxfordUltraAudioProcessor::createParameterLayout()
{
    using namespace BubLeonDSP::ParamIDs;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    const auto dbFormat = [](float v, int) { return juce::String(std::abs(v) < 0.005f ? 0.0f : v, 2) + " dB"; };
    const auto msFormat = [](float v, int) { return juce::String(v, 2) + " ms"; };
    const auto pctFormat= [](float v, int) { return juce::String( v * 100.0f, 1 ) + " %"; };
    const auto hzFormat = [](float v, int) { return juce::String(static_cast<int>(v)) + " Hz"; };

    // 1. Primary Gain & Dynamics
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { inputGain, 1 }, "Input Gain",
        juce::NormalisableRange<float>(-12.0f, 24.0f, 0.01f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(dbFormat)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { threshold, 1 }, "Ceiling",
        juce::NormalisableRange<float>(-18.0f, 0.0f, 0.01f, 1.2f), -0.10f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(dbFormat)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { attack, 1 }, "Lookahead Attack",
        juce::NormalisableRange<float>(0.10f, 10.0f, 0.01f, 0.45f), 1.50f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(msFormat)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { release, 1 }, "Release",
        juce::NormalisableRange<float>(10.0f, 1000.0f, 0.1f, 0.35f), 120.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(msFormat)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { autoRelease, 1 }, "Crest Auto-Release",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.80f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctFormat)));

    // 2. Oxford ADAA "Enhance" & Transient Restoration
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { enhance, 1 }, "Enhance (ADAA)",
        juce::NormalisableRange<float>(0.0f, 1.25f, 0.001f), 0.35f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctFormat)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { transientPunch, 1 }, "Transient Punch",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.20f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctFormat)));

    // 3. LR4 Dual-Band Split
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { crossoverHz, 1 }, "LR4 Sub Crossover",
        juce::NormalisableRange<float>(40.0f, 250.0f, 1.0f, 0.55f), 110.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(hzFormat)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { lfHeadroom, 1 }, "LF Sub Ceiling Ratio",
        juce::NormalisableRange<float>(0.50f, 1.0f, 0.001f), 0.78f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctFormat)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { bandCoupling, 1 }, "LF-to-HF Coupling",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.15f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctFormat)));

    // 4. Mid/Side Matrix & Width Enhancement
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { msMode, 1 }, "M/S Matrix Enable", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { midSideLink, 1 }, "Mid/Side Link",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctFormat)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { sideLowMono, 1 }, "Sub-Bass Mono Maker",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.65f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctFormat)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { enhanceSideBoost, 1 }, "Enhance Side Width Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.30f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctFormat)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { stereoWidth, 1 }, "Output Stereo Width",
        juce::NormalisableRange<float>(0.50f, 1.50f, 0.001f), 1.05f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctFormat)));

    // 5. Psychoacoustic Dither, Oversampling & 1:1 Loudness Match
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ditherBits, 1 }, "Dither Bit Depth",
        juce::StringArray { "Off (32-bit Float)", "24-Bit TPDF", "16-Bit TPDF" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ditherShaper, 1 }, "Noise Shaping Curve",
        juce::StringArray { "Flat High-Pass TPDF", "Modified E-Weight (Codec Safe)", "Psychoacoustic F-Weight (Type-1)" }, 2));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { autoBlanking, 1 }, "Dither Silence Auto-Blanking", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { autoGainMatch, 1 }, "1:1 Unity Loudness Match", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { oversampling, 1 }, "4x Linear-Phase Oversampling", true));

    return { params.begin(), params.end() };
}

void OxfordUltraAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlockSize = juce::jmax(1, samplesPerBlock);

    oversampler4x.initProcessing(static_cast<size_t>(maxBlockSize));
    limiter4x.prepare(sampleRate, 4);
    limiter1x.prepare(sampleRate, 1);

    syncParametersFromAPVTS();
    limiter4x.reset();          // snap gain/ceiling smoothers to the session's values (no start-up glide over the ceiling)
    limiter1x.reset();

    // Dry-path delay buffer sized for the larger engine so a 1x/4x switch never allocates
    const int latency4x = static_cast<int>(std::lround(oversampler4x.getLatencyInSamples())) + limiter4x.getInternalLatencyBaseSamples();
    const int latency1x = limiter1x.getInternalLatencyBaseSamples();
    loudnessAnalyzer.prepare(sampleRate, juce::jmax(latency4x, latency1x));
    updateLatencyForEngine();

    scopeCollector.prepare(sampleRate, 180);
    dryInputCopy.setSize(2, maxBlockSize, false, true, false);
}

void OxfordUltraAudioProcessor::updateLatencyForEngine() noexcept
{
    const int latency4x = static_cast<int>(std::lround(oversampler4x.getLatencyInSamples()))
                        + limiter4x.getInternalLatencyBaseSamples();
    setLatencySamples(lastOsActive ? latency4x : limiter1x.getInternalLatencyBaseSamples());
    loudnessAnalyzer.setLatencySamples(getLatencySamples());   // keep in/out loudness comparison aligned
}

void OxfordUltraAudioProcessor::releaseResources()
{
    oversampler4x.reset();
    limiter4x.reset();
    limiter1x.reset();
}

bool OxfordUltraAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void OxfordUltraAudioProcessor::syncParametersFromAPVTS() noexcept
{
    BubLeonDSP::LimiterParameters p;
    p.inputGainDb     = paramPtrs.inputGain->load(std::memory_order_relaxed);
    p.thresholdDb     = paramPtrs.threshold->load(std::memory_order_relaxed);
    p.attackMs        = paramPtrs.attack->load(std::memory_order_relaxed);
    p.releaseMs       = paramPtrs.release->load(std::memory_order_relaxed);
    p.autoRelease     = paramPtrs.autoRelease->load(std::memory_order_relaxed);
    p.enhanceAmount   = paramPtrs.enhance->load(std::memory_order_relaxed);
    p.transientPunch  = paramPtrs.transientPunch->load(std::memory_order_relaxed);
    p.crossoverHz     = paramPtrs.crossoverHz->load(std::memory_order_relaxed);
    p.lfHeadroomRatio = paramPtrs.lfHeadroom->load(std::memory_order_relaxed);
    p.bandCoupling    = paramPtrs.bandCoupling->load(std::memory_order_relaxed);

    const int bitsIdx   = static_cast<int>(paramPtrs.ditherBits->load(std::memory_order_relaxed));
    const int shaperIdx = static_cast<int>(paramPtrs.ditherShaper->load(std::memory_order_relaxed));
    p.ditherBits        = static_cast<BubLeonDSP::BitDepth>(std::clamp(bitsIdx, 0, 2));
    p.ditherShaper      = static_cast<BubLeonDSP::NoiseShapingCurve>(std::clamp(shaperIdx, 0, 2));
    p.autoBlanking      = paramPtrs.autoBlanking->load(std::memory_order_relaxed) > 0.5f;

    BubLeonDSP::MidSideParameters ms;
    ms.msModeEnabled    = paramPtrs.msMode->load(std::memory_order_relaxed) > 0.5f;
    ms.midSideLink      = paramPtrs.midSideLink->load(std::memory_order_relaxed);
    ms.sideLowMono      = paramPtrs.sideLowMono->load(std::memory_order_relaxed);
    ms.enhanceSideBoost = paramPtrs.enhanceSideBoost->load(std::memory_order_relaxed);
    ms.stereoWidthOut   = paramPtrs.stereoWidth->load(std::memory_order_relaxed);

    limiter4x.setParameters(p);
    limiter4x.setMidSideParameters(ms);
    limiter1x.setParameters(p);
    limiter1x.setMidSideParameters(ms);

    const bool matchOn = paramPtrs.autoGainMatch->load(std::memory_order_relaxed) > 0.5f;
    loudnessAnalyzer.setAutoGainMatchEnabled(matchOn);

    lastOsActive = paramPtrs.oversampling->load(std::memory_order_relaxed) > 0.5f;
}

void OxfordUltraAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || buffer.getNumChannels() < 2)
        return;

    const bool wasOsActive = lastOsActive;
    syncParametersFromAPVTS();

    // 4x <-> 1x engine switch: clear the incoming engine and report the new latency to the host
    if (wasOsActive != lastOsActive)
    {
        if (lastOsActive) { oversampler4x.reset(); limiter4x.reset(); }
        else              { limiter1x.reset(); }
        updateLatencyForEngine();
    }

    if (loudnessResetRequested.exchange(false, std::memory_order_acq_rel))
        loudnessAnalyzer.resetIntegrated();

    // Hosts may exceed the prepared block size: split so the oversampler and scratch
    // buffers are never overrun (and nothing is allocated on the audio thread).
    float* const* channels = buffer.getArrayOfWritePointers();
    for (int start = 0; start < numSamples; start += maxBlockSize)
    {
        const int n = juce::jmin(maxBlockSize, numSamples - start);
        juce::AudioBuffer<float> chunk(channels, 2, start, n);   // references host memory, no allocation
        processChunk(chunk);
    }
}

void OxfordUltraAudioProcessor::processChunk(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();

    // 1. Copy raw 1x input before limiting for latency-aligned LUFS comparison
    dryInputCopy.copyFrom(0, 0, buffer, 0, 0, numSamples);
    dryInputCopy.copyFrom(1, 0, buffer, 1, 0, numSamples);

    auto& activeLimiter = lastOsActive ? limiter4x : limiter1x;

    // 2. Process Limiting Core (4x Oversampled or 1x Low-Latency Base Rate)
    if (lastOsActive)
    {
        juce::dsp::AudioBlock<float> baseBlock(buffer);
        juce::dsp::AudioBlock<float> osBlock = oversampler4x.processSamplesUp(baseBlock);

        float* osL = osBlock.getChannelPointer(0);
        float* osR = osBlock.getChannelPointer(1);
        const size_t numOsSamples = osBlock.getNumSamples();

        for (size_t i = 0; i < numOsSamples; ++i)
            limiter4x.processOversampledStereo(osL[i], osR[i]);

        oversampler4x.processSamplesDown(baseBlock);
    }
    else
    {
        float* outL = buffer.getWritePointer(0);
        float* outR = buffer.getWritePointer(1);
        for (int i = 0; i < numSamples; ++i)
            limiter1x.processOversampledStereo(outL[i], outR[i]);
    }

    // 3. Base-Rate ITU-R BS.1770-4 LUFS/PLR Analysis, Unity Gain Match & Psychoacoustic Dither
    const float* rawL = dryInputCopy.getReadPointer(0);
    const float* rawR = dryInputCopy.getReadPointer(1);
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);

    const auto tele = activeLimiter.getTelemetry();
    const float tpLin = std::pow(10.0f, tele.outputTruePeakDb * 0.05f);
    const float inGainDb = paramPtrs.inputGain->load(std::memory_order_relaxed);
    const float ceilDb   = paramPtrs.threshold->load(std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        loudnessAnalyzer.processSampleStereo(rawL[i], rawR[i], outL[i], outR[i], tpLin);
        activeLimiter.processBaseRateDitherStereo(outL[i], outR[i]);
        scopeCollector.pushBaseRateSample(rawL[i], rawR[i], outL[i], outR[i], inGainDb, ceilDb, tele);
    }
}

int OxfordUltraAudioProcessor::getNumPrograms()
{
    return BubLeonPresets::count();
}

int OxfordUltraAudioProcessor::getCurrentProgram()
{
    return juce::jlimit(0, BubLeonPresets::count() - 1, currentProgram.load(std::memory_order_relaxed));
}

void OxfordUltraAudioProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= BubLeonPresets::count())
        return;
    BubLeonPresets::apply(apvts, index);
    currentProgram.store(index, std::memory_order_relaxed);
    apvts.state.setProperty("presetIndex", index, nullptr);
    updateHostDisplay(ChangeDetails().withProgramChanged(true));
}

const juce::String OxfordUltraAudioProcessor::getProgramName(int index)
{
    if (index < 0 || index >= BubLeonPresets::count())
        return {};
    return BubLeonPresets::all()[(size_t) index].name;
}

void OxfordUltraAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void OxfordUltraAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        currentProgram.store(juce::jlimit(0, BubLeonPresets::count() - 1, (int) apvts.state.getProperty("presetIndex", 0)),
                             std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* OxfordUltraAudioProcessor::createEditor()
{
    return new OxfordUltraAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OxfordUltraAudioProcessor();
}
