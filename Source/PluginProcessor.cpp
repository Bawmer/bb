#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets/FactoryPresets.h"

using namespace bl;

namespace
{
const juce::Identifier kOrderId ("order"), kUiScaleId ("uiScale");
juce::Identifier typeId (int slot) { return juce::Identifier ("type" + juce::String (slot)); }

/** Generic slot control whose name and value text follow the module loaded in its slot. */
class SlotParam final : public juce::AudioParameterFloat
{
public:
    SlotParam (int s, int k, const std::array<std::atomic<int>, kNumSlots>& types)
        : juce::AudioParameterFloat (juce::ParameterID { MixRackProcessor::slotId (s, "p" + juce::String (k + 1)), 1 },
                                     "S" + juce::String (s + 1) + " P" + juce::String (k + 1),
                                     juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f),
          slot (s), index (k), slotTypes (types) {}

    const ParamSpec* spec() const
    {
        if (auto* mi = ModuleRegistry::info (slotTypes[(size_t) slot].load()))
            if (index < (int) mi->params.size()) return &mi->params[(size_t) index];
        return nullptr;
    }

    juce::String getName (int maxLen) const override
    {
        juce::String n = "S" + juce::String (slot + 1) + " ";
        if (auto* mi = ModuleRegistry::info (slotTypes[(size_t) slot].load()))
            n << mi->shortName << " " << (spec() ? juce::String (spec()->name) : juce::String ("-"));
        else
            n << "P" << (index + 1);
        return n.substring (0, maxLen);
    }

    juce::String getText (float v, int) const override
    {
        if (auto* s = spec()) return s->toText (s->toReal (v));
        return "-";
    }

    float getValueForText (const juce::String& text) const override
    {
        auto* s = spec();
        if (s == nullptr) return text.getFloatValue();
        if (s->kind == ParamSpec::Kind::Choice)
        {
            for (size_t i = 0; i < s->choices.size(); ++i)
                if (text.trim().equalsIgnoreCase (juce::String (s->choices[i]))) return (float) s->toNorm ((double) i);
            return (float) s->toNorm (text.getIntValue());
        }
        if (s->kind == ParamSpec::Kind::Toggle)
            return text.trim().equalsIgnoreCase ("on") || text.getIntValue() > 0 ? 1.0f : 0.0f;
        if (text.trim().equalsIgnoreCase (juce::String (s->offText.empty() ? std::string ("Off") : s->offText)))
            return s->offAtMin ? 0.0f : 1.0f;
        double v = text.getDoubleValue();
        if (text.containsIgnoreCase ("k")) v *= 1000.0;
        if (s->unit == "ms" && text.containsIgnoreCase ("us")) v *= 0.001;
        else if (s->unit == "ms" && text.trim().endsWithIgnoreCase ("s") && ! text.containsIgnoreCase ("ms")) v *= 1000.0;
        return (float) s->toNorm (v);
    }

private:
    int slot, index;
    const std::array<std::atomic<int>, kNumSlots>& slotTypes;
};

juce::String dbText (float v, int) { return juce::String (v, 1) + " dB"; }
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MixRackProcessor::createLayout (MixRackProcessor& p)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("slot" + juce::String (s + 1), "Slot " + juce::String (s + 1), "|");
        for (int k = 0; k < kParamsPerSlot; ++k)
            group->addChild (std::make_unique<SlotParam> (s, k, p.slotType));
        const auto pre = "S" + juce::String (s + 1) + " ";
        group->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { slotId (s, "bypass"), 1 }, pre + "Bypass", false));
        group->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { slotId (s, "solo"), 1 }, pre + "Solo", false));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { slotId (s, "mix"), 1 }, pre + "Mix",
                         juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                         juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0) + " %"; })));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { slotId (s, "in"), 1 }, pre + "Input Trim",
                         juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { slotId (s, "out"), 1 }, pre + "Output Trim",
                         juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { slotId (s, "os"), 1 }, pre + "Oversampling",
                         juce::StringArray { "Off", "2x", "4x" }, 0));
        layout.add (std::move (group));
    }
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "in_gain", 1 }, "Input Gain",
                juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "out_gain", 1 }, "Output Gain",
                juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "os_mode", 1 }, "Oversampling Mode",
                juce::StringArray { "Per Slot", "Off", "2x", "4x" }, 0));
    return layout;
}

//==============================================================================
MixRackProcessor::MixRackProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "MixRack", createLayout (*this))
{
    for (auto& t : slotType) t.store (-1);

    for (int s = 0; s < kNumSlots; ++s)
    {
        SlotParamRefs r;
        for (int k = 0; k < kParamsPerSlot; ++k) r.norm[k] = apvts.getRawParameterValue (slotId (s, "p" + juce::String (k + 1)));
        r.bypass  = apvts.getRawParameterValue (slotId (s, "bypass"));
        r.solo    = apvts.getRawParameterValue (slotId (s, "solo"));
        r.mix     = apvts.getRawParameterValue (slotId (s, "mix"));
        r.inTrim  = apvts.getRawParameterValue (slotId (s, "in"));
        r.outTrim = apvts.getRawParameterValue (slotId (s, "out"));
        soloRefs[s] = r.solo;
        osRefs[s] = apvts.getRawParameterValue (slotId (s, "os"));
        engine.slot (s).setParamRefs (r);
    }
    inGainRef   = apvts.getRawParameterValue ("in_gain");
    outGainRef  = apvts.getRawParameterValue ("out_gain");
    osGlobalRef = apvts.getRawParameterValue ("os_mode");

    engine.prepare (sr, maxBlock);
    inMeter.prepare (sr); outMeter.prepare (sr);
    work[0].assign ((size_t) maxBlock, 0.0); work[1].assign ((size_t) maxBlock, 0.0);

    apvts.state.setProperty (kOrderId, "0,1,2,3,4,5,6,7", nullptr);
    for (int s = 0; s < kNumSlots; ++s) apvts.state.setProperty (typeId (s), "", nullptr);
    apvts.state.addListener (this);
    loadInitRack();
    undoManager.clearUndoHistory();

    startTimerHz (15);
}

MixRackProcessor::~MixRackProcessor()
{
    stopTimer();
    apvts.state.removeListener (this);
}

juce::File MixRackProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("BL Audio").getChildFile ("Mix Rack").getChildFile ("Presets");
}

//==============================================================================
bool MixRackProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const auto ok = [] (const juce::AudioChannelSet& s) { return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo(); };
    if (! ok (in) || ! ok (out)) return false;
    // mono->mono, stereo->stereo, stereo->mono, mono->stereo
    return true;
}

void MixRackProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    std::lock_guard<std::recursive_mutex> lock (installMutex);
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    maxBlock = juce::jmax (16, samplesPerBlock);
    work[0].assign ((size_t) maxBlock, 0.0); work[1].assign ((size_t) maxBlock, 0.0);
    engine.prepare (sr, maxBlock);
    for (int s = 0; s < kNumSlots; ++s)
    {
        rebuildSlot (s, slotType[(size_t) s].load());
        engine.slot (s).adoptPendingNow();
    }
    inMeter.prepare (sr); outMeter.prepare (sr);
    inGainS.reset (sr, 0.03); outGainS.reset (sr, 0.03);
    inGainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain ((double) inGainRef->load()));
    outGainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain ((double) outGainRef->load()));
    setLatencySamples (engine.totalLatency());
    prepared = true;
}

void MixRackProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)  { processT (b); }
void MixRackProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processT (b); }

template <typename T>
void MixRackProcessor::processT (juce::AudioBuffer<T>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    const int total = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    bool soloActive = false;
    for (auto* s : soloRefs) soloActive |= s->load() > 0.5f;
    inGainS.setTargetValue (juce::Decibels::decibelsToGain ((double) inGainRef->load(), -120.0));
    outGainS.setTargetValue (juce::Decibels::decibelsToGain ((double) outGainRef->load(), -120.0));

    double* l = work[0].data();
    double* r = work[1].data();

    for (int off = 0; off < total; off += maxBlock)
    {
        const int n = juce::jmin (maxBlock, total - off);
        const T* inL = inCh > 0 && numCh > 0 ? buffer.getReadPointer (0, off) : nullptr;
        const T* inR = inCh > 1 && numCh > 1 ? buffer.getReadPointer (1, off) : inL;
        for (int i = 0; i < n; ++i)
        {
            l[i] = inL ? (double) inL[i] : 0.0;
            r[i] = inR ? (double) inR[i] : 0.0;
        }
        if (inGainS.isSmoothing() || inGainS.getTargetValue() != 1.0)
            for (int i = 0; i < n; ++i) { const double g = inGainS.getNextValue(); l[i] *= g; r[i] *= g; }
        inMeter.process (l, r, n);

        engine.process (l, r, n, soloActive);

        if (outGainS.isSmoothing() || outGainS.getTargetValue() != 1.0)
            for (int i = 0; i < n; ++i) { const double g = outGainS.getNextValue(); l[i] *= g; r[i] *= g; }
        outMeter.process (l, r, n);

        if (outCh == 1 && numCh > 0)
        {
            T* o = buffer.getWritePointer (0, off);
            for (int i = 0; i < n; ++i) o[i] = (T) ((l[i] + r[i]) * 0.5);
        }
        else if (numCh > 1)
        {
            T* oL = buffer.getWritePointer (0, off);
            T* oR = buffer.getWritePointer (1, off);
            for (int i = 0; i < n; ++i) { oL[i] = (T) l[i]; oR[i] = (T) r[i]; }
        }
        for (int c = juce::jmax (outCh, 2); c < numCh; ++c) buffer.clear (c, off, n);
    }

    if (total > 0)
    {
        const double elapsed = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        const double load = elapsed / (total / sr);
        cpuLoad.store ((float) (cpuLoad.load() * 0.9 + load * 0.1));
    }
}

//==============================================================================
int MixRackProcessor::desiredOsIndex (int slot) const
{
    const int g = (int) osGlobalRef->load();
    if (g == 1) return 0;
    if (g == 2) return 1;
    if (g == 3) return 2;
    return juce::jlimit (0, 2, (int) osRefs[slot]->load());
}

void MixRackProcessor::rebuildSlot (int slot, int type)
{
    std::lock_guard<std::recursive_mutex> lock (installMutex);
    engine.slot (slot).install (engine.slot (slot).build (type, desiredOsIndex (slot)));
}

void MixRackProcessor::syncFromMessageThread()
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& sl = engine.slot (s);
        sl.collectGarbage();
        const int type = slotType[(size_t) s].load();
        int want = desiredOsIndex (s);
        if (auto* mi = ModuleRegistry::info (type); mi == nullptr || ! mi->supportsOversampling) want = 0;
        if (sl.getInstalledType() != type || sl.getInstalledOs() != want)
            rebuildSlot (s, type);
        if (! prepared) sl.adoptPendingNow();
    }
    const int lat = engine.totalLatency();
    if (lat != getLatencySamples()) setLatencySamples (lat);
}

//==============================================================================
void MixRackProcessor::applyTypeFromTree (int slot)
{
    const auto id = apvts.state.getProperty (typeId (slot)).toString();
    const int type = id.isEmpty() ? -1 : ModuleRegistry::indexOf (id.toStdString());
    if (slotType[(size_t) slot].load() == type && engine.slot (slot).getInstalledType() == type) return;
    slotType[(size_t) slot].store (type);
    rebuildSlot (slot, type);
    if (! prepared) engine.slot (slot).adoptPendingNow();
    setLatencySamples (engine.totalLatency());
    updateHostDisplay (ChangeDetails().withParameterInfoChanged (true));
}

void MixRackProcessor::applyOrderFromTree()
{
    std::array<int, kNumSlots> order {};
    auto tokens = juce::StringArray::fromTokens (apvts.state.getProperty (kOrderId).toString(), ",", "");
    std::array<bool, kNumSlots> used {};
    bool valid = tokens.size() == kNumSlots;
    for (int p = 0; valid && p < kNumSlots; ++p)
    {
        const int v = tokens[p].getIntValue();
        if (v < 0 || v >= kNumSlots || used[(size_t) v]) valid = false;
        else { order[(size_t) p] = v; used[(size_t) v] = true; }
    }
    if (! valid) for (int p = 0; p < kNumSlots; ++p) order[(size_t) p] = p;
    engine.setOrder (order);
}

void MixRackProcessor::applyAllFromTree()
{
    for (int s = 0; s < kNumSlots; ++s) applyTypeFromTree (s);
    applyOrderFromTree();
}

void MixRackProcessor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop)
{
    if (tree != apvts.state) return;
    if (prop == kOrderId) { applyOrderFromTree(); return; }
    for (int s = 0; s < kNumSlots; ++s)
        if (prop == typeId (s)) { applyTypeFromTree (s); return; }
}

void MixRackProcessor::valueTreeRedirected (juce::ValueTree&) { applyAllFromTree(); }

//==============================================================================
juce::RangedAudioParameter* MixRackProcessor::slotParam (int slot, const juce::String& suffix) const
{
    return apvts.getParameter (slotId (slot, suffix));
}

void MixRackProcessor::resetSlotParams (int slot)
{
    const auto* mi = ModuleRegistry::info (slotType[(size_t) slot].load());
    for (int k = 0; k < kParamsPerSlot; ++k)
    {
        auto* p = slotParam (slot, "p" + juce::String (k + 1));
        const float v = (mi && k < (int) mi->params.size()) ? (float) mi->params[(size_t) k].defaultNorm() : 0.5f;
        p->setValueNotifyingHost (v);
    }
    for (auto suffix : { "bypass", "solo", "mix", "in", "out", "os" })
    {
        auto* p = slotParam (slot, suffix);
        p->setValueNotifyingHost (p->getDefaultValue());
    }
}

void MixRackProcessor::setSlotType (int slot, int type, bool resetParams)
{
    const auto* mi = ModuleRegistry::info (type);
    apvts.state.setProperty (typeId (slot), mi ? juce::String (mi->id) : juce::String(), &undoManager);
    applyTypeFromTree (slot);
    if (resetParams) resetSlotParams (slot);
}

void MixRackProcessor::moveSlotToPosition (int slot, int newPos)
{
    auto order = engine.getOrder();
    std::vector<int> v (order.begin(), order.end());
    v.erase (std::find (v.begin(), v.end(), slot));
    v.insert (v.begin() + juce::jlimit (0, (int) v.size(), newPos), slot);
    juce::StringArray s;
    for (int x : v) s.add (juce::String (x));
    apvts.state.setProperty (kOrderId, s.joinIntoString (","), &undoManager);
}

//==============================================================================
juce::ValueTree MixRackProcessor::makeModuleState (int slot) const
{
    juce::ValueTree t ("BLModule");
    const auto* mi = ModuleRegistry::info (slotType[(size_t) slot].load());
    t.setProperty ("id", mi ? juce::String (mi->id) : juce::String(), nullptr);
    t.setProperty ("version", 1, nullptr);
    if (mi)
        for (size_t k = 0; k < mi->params.size(); ++k)
        {
            juce::ValueTree p ("P");
            const auto norm = slotParam (slot, "p" + juce::String ((int) k + 1))->getValue();
            p.setProperty ("name", juce::String (mi->params[k].name), nullptr);
            p.setProperty ("value", mi->params[k].toReal (norm), nullptr);
            t.appendChild (p, nullptr);
        }
    for (auto suffix : { "mix", "in", "out", "os" })
        t.setProperty (suffix, slotParam (slot, suffix)->convertFrom0to1 (slotParam (slot, suffix)->getValue()), nullptr);
    return t;
}

void MixRackProcessor::loadModuleState (int slot, const juce::ValueTree& t)
{
    if (! t.hasType ("BLModule")) return;
    const int type = ModuleRegistry::indexOf (t["id"].toString().toStdString());
    setSlotType (slot, type, true);
    const auto* mi = ModuleRegistry::info (type);
    if (mi == nullptr) return;
    for (const auto& c : t)
    {
        const auto name = c["name"].toString().toStdString();
        for (size_t k = 0; k < mi->params.size(); ++k)
            if (mi->params[k].name == name)
                slotParam (slot, "p" + juce::String ((int) k + 1))->setValueNotifyingHost ((float) mi->params[k].toNorm ((double) c["value"]));
    }
    for (auto suffix : { "mix", "in", "out", "os" })
        if (t.hasProperty (suffix))
        {
            auto* p = slotParam (slot, suffix);
            p->setValueNotifyingHost (p->convertTo0to1 ((float) (double) t[suffix]));
        }
}

//==============================================================================
void MixRackProcessor::applyRackPreset (const RackPresetDef& def)
{
    undoManager.beginNewTransaction ("Load preset");
    std::vector<int> order;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const SlotPresetDef* sd = s < (int) def.slots.size() ? &def.slots[(size_t) s] : nullptr;
        const int type = (sd && sd->module) ? ModuleRegistry::indexOf (sd->module) : -1;
        setSlotType (s, type, true);
        if (sd == nullptr || type < 0) continue;
        const auto* mi = ModuleRegistry::info (type);
        for (const auto& [name, value] : sd->values)
            for (size_t k = 0; k < mi->params.size(); ++k)
                if (mi->params[k].name == name)
                    slotParam (s, "p" + juce::String ((int) k + 1))->setValueNotifyingHost ((float) mi->params[k].toNorm (value));
        auto setReal = [&] (const char* suffix, double v) { auto* p = slotParam (s, suffix); p->setValueNotifyingHost (p->convertTo0to1 ((float) v)); };
        setReal ("mix", sd->mix); setReal ("in", sd->inTrim); setReal ("out", sd->outTrim); setReal ("os", sd->os);
    }
    apvts.state.setProperty (kOrderId, "0,1,2,3,4,5,6,7", &undoManager);
    for (auto [pid, v] : { std::pair<const char*, double> { "in_gain", def.inGain }, { "out_gain", def.outGain } })
    {
        auto* p = apvts.getParameter (pid);
        p->setValueNotifyingHost (p->convertTo0to1 ((float) v));
    }
    currentPresetName = juce::String (def.bank) + " / " + def.name;
}

void MixRackProcessor::loadInitRack()
{
    static const RackPresetDef init { "Init", "Init", {
        { "eq4", {} }, { "vca", {} }, { nullptr, {} }, { nullptr, {} },
        { nullptr, {} }, { nullptr, {} }, { nullptr, {} }, { "limiter", {} } } };
    applyRackPreset (init);
    currentPresetName = "Init";
}

bool MixRackProcessor::saveUserPreset (const juce::File& f)
{
    auto state = apvts.copyState();
    state.removeProperty (kUiScaleId, nullptr);
    if (auto xml = state.createXml())
    {
        f.getParentDirectory().createDirectory();
        if (xml->writeTo (f)) { currentPresetName = "User / " + f.getFileNameWithoutExtension(); return true; }
    }
    return false;
}

bool MixRackProcessor::loadUserPreset (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    const auto scale = apvts.state.getProperty (kUiScaleId);
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    if (! scale.isVoid()) apvts.state.setProperty (kUiScaleId, scale, nullptr);
    currentPresetName = "User / " + f.getFileNameWithoutExtension();
    return true;
}

//==============================================================================
void MixRackProcessor::selectAB (int which)
{
    if (which == activeAB) return;
    abState[activeAB] = apvts.copyState();
    activeAB = which;
    if (abState[activeAB].isValid())
        apvts.replaceState (abState[activeAB].createCopy());
}

void MixRackProcessor::copyCurrentToOther() { abState[1 - activeAB] = apvts.copyState(); }

//==============================================================================
void MixRackProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void MixRackProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto t = juce::ValueTree::fromXml (*xml);
            currentPresetName = t.getProperty ("presetName", "Init").toString();
            apvts.replaceState (t);
        }
}

juce::AudioProcessorEditor* MixRackProcessor::createEditor() { return new MixRackEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MixRackProcessor(); }
