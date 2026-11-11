#include "PluginEditor.h"
#include "BinaryData.h"
#include "Presets.h"

namespace
{
    using namespace BubLeonDSP::ParamIDs;

    // Faceplate geometry (pixels in Resources/faceplate.png, 1295 x 832)
    constexpr float kRow1Y = 560.0f, kRow2Y = 727.0f;
    constexpr float kRow1X[7] = { 110, 289, 466, 644, 822, 1000, 1178 };
    constexpr float kRow2X[7] = { 110, 289, 466, 644, 823, 1001, 1179 };
    constexpr float kBox1X[7] = { 62, 240, 418, 595, 773, 950, 1128 };
    constexpr float kBox2X[7] = { 61, 239, 417, 595, 773, 951, 1129 };

    const char* const kParamOrder[14] = {
        inputGain, threshold, enhance, transientPunch, attack, release, autoRelease,
        crossoverHz, lfHeadroom, bandCoupling, midSideLink, sideLowMono, enhanceSideBoost, stereoWidth
    };

    const char* const kTooltips[14] = {
        "Input gain into the limiter", "Output ceiling (dBTP)", "ADAA tanh loudness enhancer",
        "Transient punch on the mid/high band", "Lookahead ramp length", "Release time",
        "Crest-factor adaptive release amount", "LR4 sub-band crossover", "LF band ceiling relative to main ceiling",
        "Share of LF gain reduction applied to the HF band", "L/R detector link", "Mono-ize side content below the crossover",
        "Extra Enhance drive on the side channel", "Output stereo width"
    };

    const char* const kMacroParams[3] = { inputGain, threshold, enhance };
    constexpr float kMacroY[3] = { 188.0f, 289.0f, 391.0f };
    constexpr float kMacroX[3] = { 945.0f, 945.0f, 946.0f };

    const juce::StringArray kBitNames   { "32-Bit Float", "24-Bit Dither", "16-Bit Dither" };
    const juce::StringArray kShapeNames { "Flat HP TPDF", "E-Weight (Codec)", "F-Weight (Type-1)" };
    const juce::StringArray kShapeShort { "Flat", "E-Wt", "F-Wt" };

    const juce::Identifier kUiScale { "uiScale" }, kScopeRange { "scopeRange" }, kScopeTime { "scopeTime" };
}

OxfordUltraAudioProcessorEditor::OxfordUltraAudioProcessorEditor(OxfordUltraAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      historyScope(p.scopeCollector)
{
    setLookAndFeel(&lnf);
    faceplate = juce::ImageCache::getFromMemory(BinaryData::faceplate_png, BinaryData::faceplate_pngSize);
    setOpaque(true);

    addAndMakeVisible(historyScope);
    addAndMakeVisible(hud);

    auto& apvts = processorRef.apvts;
    auto setupDefault = [&apvts](juce::Slider& s, const char* id)
    {
        if (auto* param = apvts.getParameter(id))
            s.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
    };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        addAndMakeVisible(k.slider);
        addAndMakeVisible(k.box);
        k.attachment = std::make_unique<SliderAttachment>(apvts, kParamOrder[i], k.slider);
        setupDefault(k.slider, kParamOrder[i]);
        k.slider.setTooltip(kTooltips[i]);
        k.box.text = [&s = k.slider] { return s.getTextFromValue(s.getValue()); };
        k.slider.onValueChange = [&b = k.box] { b.repaint(); };
    }

    // WIDTH OUT box shows the dither mode (as on the faceplate); hover/drag the knob to read width.
    {
        auto& w = knobs[13];
        w.box.text = [this, &s = w.slider]
        {
            if (s.isMouseOverOrDragging())
                return s.getTextFromValue(s.getValue());
            const int bits = (int) processorRef.apvts.getRawParameterValue(ditherBits)->load();
            return kBitNames[juce::jlimit(0, 2, bits)];
        };
        w.box.onClick = [this] { showDitherMenu(); };
        w.box.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        w.box.setTooltip("Click: dither bit depth");
    }

    for (size_t i = 0; i < macros.size(); ++i)
    {
        auto& m = macros[i];
        addAndMakeVisible(m.slider);
        m.attachment = std::make_unique<SliderAttachment>(apvts, kMacroParams[i], m.slider);
        setupDefault(m.slider, kMacroParams[i]);
        m.slider.setPopupDisplayEnabled(true, true, this);
    }

    // Big caps: scope view controls (GUI-only, saved with the session)
    auto setupBig = [this](BubLeonGui::FaceKnob& s, float faceDx, float faceDy)
    {
        s.getProperties().set("bigKnob", true);
        s.getProperties().set("faceDx", faceDx);
        s.getProperties().set("faceDy", faceDy);
        s.setPopupDisplayEnabled(true, true, this);
        addAndMakeVisible(s);
    };
    setupBig(scopeRangeKnob, -12.5f, -12.5f);
    setupBig(scopeTimeKnob,  -10.0f,  -7.5f);

    scopeRangeKnob.setRange(12.0, 60.0, 1.0);
    scopeRangeKnob.textFromValueFunction = [](double v) { return "Scope range " + juce::String((int) v) + " dB"; };
    scopeRangeKnob.setTooltip("Scope level range");
    scopeRangeKnob.setValue((double) apvts.state.getProperty(kScopeRange, 36.0), juce::dontSendNotification);
    scopeRangeKnob.setDoubleClickReturnValue(true, 36.0);
    scopeRangeKnob.onValueChange = [this]
    {
        historyScope.setRangeDb((float) scopeRangeKnob.getValue());
        processorRef.apvts.state.setProperty(kScopeRange, scopeRangeKnob.getValue(), nullptr);
    };

    scopeTimeKnob.setRange(1.0, 10.0, 0.5);
    scopeTimeKnob.textFromValueFunction = [](double v) { return "Scope time " + juce::String(v, 1) + " s"; };
    scopeTimeKnob.setTooltip("Scope history length");
    scopeTimeKnob.setValue((double) apvts.state.getProperty(kScopeTime, 3.0), juce::dontSendNotification);
    scopeTimeKnob.setDoubleClickReturnValue(true, 3.0);
    scopeTimeKnob.onValueChange = [this]
    {
        historyScope.setTimeSeconds((float) scopeTimeKnob.getValue());
        processorRef.apvts.state.setProperty(kScopeTime, scopeTimeKnob.getValue(), nullptr);
    };
    historyScope.setRangeDb((float) scopeRangeKnob.getValue());
    historyScope.setTimeSeconds((float) scopeTimeKnob.getValue());

    for (auto* b : { &msToggle, &matchToggle, &osToggle })
        addAndMakeVisible(*b);
    msToggle.setTooltip("M/S matrix: sub mono, side Enhance drive");
    matchToggle.setTooltip("1:1 loudness match (monitor at input loudness)");
    osToggle.setTooltip("4x linear-phase oversampling");
    msAttach    = std::make_unique<ButtonAttachment>(apvts, msMode, msToggle);
    matchAttach = std::make_unique<ButtonAttachment>(apvts, autoGainMatch, matchToggle);
    osAttach    = std::make_unique<ButtonAttachment>(apvts, oversampling, osToggle);

    addAndMakeVisible(lcd);
    lcd.onClick = [this] { showSettingsMenu(); };
    lcd.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    lcd.setTooltip("Click: presets, dither, noise shaping, window size");

    const float scale = (float) (double) apvts.state.getProperty(kUiScale, 0.85);
    setResizable(true, true);
    setResizeLimits((int) (kFaceW * 0.55f), (int) (kFaceH * 0.55f), (int) (kFaceW * 1.4f), (int) (kFaceH * 1.4f));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio((double) kFaceW / (double) kFaceH);
    setSize((int) std::lround(kFaceW * scale), (int) std::lround(kFaceH * scale));

    startTimerHz(30);
}

OxfordUltraAudioProcessorEditor::~OxfordUltraAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

juce::Rectangle<int> OxfordUltraAudioProcessorEditor::face(float x, float y, float w, float h) const
{
    const float s = (float) getWidth() / (float) kFaceW;
    return juce::Rectangle<float>(x * s, y * s, w * s, h * s).getSmallestIntegerContainer();
}

juce::Rectangle<int> OxfordUltraAudioProcessorEditor::knobBounds(float cx, float cy) const
{
    const float s = (float) getWidth() / (float) kFaceW;
    const int size = (int) std::lround(2.0f * BubLeonGui::kKnobHalf * s);
    return juce::Rectangle<int>(size, size).withCentre({ (int) std::lround(cx * s), (int) std::lround(cy * s) });
}

void OxfordUltraAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(faceplate, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

void OxfordUltraAudioProcessorEditor::resized()
{
    for (size_t i = 0; i < 7; ++i)
    {
        knobs[i].slider.setBounds(knobBounds(kRow1X[i], kRow1Y));
        knobs[i].box.setBounds(face(kBox1X[i] + 2, 614, 96, 24));
        knobs[i + 7].slider.setBounds(knobBounds(kRow2X[i], kRow2Y));
        knobs[i + 7].box.setBounds(face(kBox2X[i] + 2, 780, 96, 23));
    }
    for (size_t i = 0; i < 3; ++i)
        macros[i].slider.setBounds(knobBounds(kMacroX[i], kMacroY[i]));

    scopeRangeKnob.setBounds(knobBounds(82.0f, 298.0f));
    scopeTimeKnob.setBounds(knobBounds(835.0f, 295.0f));

    historyScope.setBounds(face(142, 183, 636, 245));
    hud.setBounds(face(1012, 149, 242, 247));

    msToggle.setBounds(face(1050 - 42, 399, 84, 50));
    matchToggle.setBounds(face(1135 - 42, 399, 84, 50));
    osToggle.setBounds(face(1220 - 42, 399, 84, 50));

    lcd.setBounds(face(955, 38, 198, 56));

    processorRef.apvts.state.setProperty(kUiScale, (double) getWidth() / (double) kFaceW, nullptr);
}

void OxfordUltraAudioProcessorEditor::timerCallback()
{
    auto& apvts = processorRef.apvts;
    const float ceilDb = apvts.getRawParameterValue(threshold)->load();
    const auto loud = processorRef.getLatestLoudnessTelemetry();
    const auto& frame = historyScope.getLatestFrame();

    historyScope.setCeilingDb(ceilDb);
    hud.update(loud, frame, ceilDb);

    juce::String status;
    if (loud.truePeakDb > ceilDb + 0.05f)  status = "TP OVER CEILING";
    else if (frame.grDb > 0.1f)            status = "GR -" + juce::String(frame.grDb, 1) + " dB  " + (osToggle.getToggleState() ? "4x" : "1x");
    else                                   status = "All Systems Nominal";

    const int bits  = juce::jlimit(0, 2, (int) apvts.getRawParameterValue(ditherBits)->load());
    const int shape = juce::jlimit(0, 2, (int) apvts.getRawParameterValue(ditherShaper)->load());
    const int preset = processorRef.getCurrentProgram();
    const juce::String l1 = juce::String(BubLeonPresets::all()[(size_t) preset].name)
                          + (BubLeonPresets::matches(apvts, preset) ? "" : " *");
    const juce::String l3 = "Settings...  " + (bits == 0 ? juce::String("32f") : (bits == 1 ? juce::String("24b ") : juce::String("16b ")) + kShapeShort[shape]);

    if (l1 != lcd.line1 || status != lcd.line2 || l3 != lcd.line3)
    {
        lcd.line1 = l1;
        lcd.line2 = status;
        lcd.line3 = l3;
        lcd.repaint();
    }
    knobs[13].box.repaint();
}

void OxfordUltraAudioProcessorEditor::LcdDisplay::paint(juce::Graphics& g)
{
    const float u = (float) getWidth() / 198.0f;
    const float w = (float) getWidth();
    g.setColour(BubLeonGui::Colours::lcdText);
    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 15.5f * u, juce::Font::plain));
    const float lh = 17.0f * u;
    g.drawText(line1, juce::Rectangle<float>(4 * u, 2 * u,          w - 6 * u, lh), juce::Justification::left);
    g.drawText(line2, juce::Rectangle<float>(4 * u, 2 * u + lh,     w - 6 * u, lh), juce::Justification::left);
    g.drawText(line3, juce::Rectangle<float>(4 * u, 2 * u + 2 * lh, w - 6 * u, lh), juce::Justification::left);
}

void OxfordUltraAudioProcessorEditor::setChoiceParam(const char* paramID, int index)
{
    if (auto* param = processorRef.apvts.getParameter(paramID))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1((float) index));
        param->endChangeGesture();
    }
}

void OxfordUltraAudioProcessorEditor::showDitherMenu()
{
    const int bits = juce::jlimit(0, 2, (int) processorRef.apvts.getRawParameterValue(ditherBits)->load());
    juce::PopupMenu m;
    for (int i = 0; i < 3; ++i)
        m.addItem(kBitNames[i], true, i == bits, [this, i] { setChoiceParam(ditherBits, i); });
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&knobs[13].box));
}

void OxfordUltraAudioProcessorEditor::showSettingsMenu()
{
    auto& apvts = processorRef.apvts;
    const int bits  = juce::jlimit(0, 2, (int) apvts.getRawParameterValue(ditherBits)->load());
    const int shape = juce::jlimit(0, 2, (int) apvts.getRawParameterValue(ditherShaper)->load());
    const bool blank = apvts.getRawParameterValue(autoBlanking)->load() > 0.5f;

    juce::PopupMenu dither, shaping, size;
    for (int i = 0; i < 3; ++i)
    {
        dither.addItem(kBitNames[i], true, i == bits, [this, i] { setChoiceParam(ditherBits, i); });
        shaping.addItem(kShapeNames[i], true, i == shape, [this, i] { setChoiceParam(ditherShaper, i); });
    }
    for (float s : { 0.7f, 0.85f, 1.0f, 1.2f })
        size.addItem(juce::String((int) std::lround(s * 100)) + " %", true,
                     std::abs((float) getWidth() / kFaceW - s) < 0.01f,
                     [this, s] { setSize((int) std::lround(kFaceW * s), (int) std::lround(kFaceH * s)); });

    juce::PopupMenu presets;
    const int current = processorRef.getCurrentProgram();
    for (int i = 0; i < BubLeonPresets::count(); ++i)
        presets.addItem(BubLeonPresets::all()[(size_t) i].name, true, i == current,
                        [this, i] { processorRef.setCurrentProgram(i); });

    juce::PopupMenu m;
    m.addSectionHeader("BL OXFORD ULTRA  v" JucePlugin_VersionString);
    m.addSubMenu("Presets", presets);
    m.addItem("Load 'Pro Engineer'", [this] { processorRef.setCurrentProgram(1); });
    m.addSeparator();
    m.addSubMenu("Dither bit depth", dither);
    m.addSubMenu("Noise shaping", shaping);
    m.addItem("Dither silence auto-blanking", true, blank, [this, blank] { setChoiceParam(autoBlanking, blank ? 0 : 1); });
    m.addSeparator();
    m.addItem("Reset integrated loudness + TP max", [this] { processorRef.requestLoudnessReset(); });
    m.addSubMenu("Window size", size);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&lcd));
}
