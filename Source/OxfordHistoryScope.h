// OxfordHistoryScope.h — BUB LEON Oxford Ultra Mastering Limiter
// 60 Hz history scope (faceplate glass screen) + loudness HUD. JUCE 8 renders through Direct2D on Windows.
//   Upper lane: input level (after input gain), output level, ceiling line.
//   Lower lane: gain-reduction history — total (amber) and LF band (cyan), 0..-12 dB.
// ScopeDataCollector is a wait-free SPSC bridge (audio thread -> message thread).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "OxfordUltraLimiter.h"
#include "LoudnessMeterITU.h"

namespace BubLeonDSP
{
    struct ScopeFrame
    {
        float inputDb  = -120.0f;   // peak of the column, input gain applied
        float outputDb = -120.0f;   // measured output peak of the column
        float grDb     = 0.0f;
        float lfGrDb   = 0.0f;
        float ceilDb   = 0.0f;
    };

    class ScopeDataCollector
    {
    public:
        static constexpr int kFifoSize = 512;

        void prepare (double sampleRate, int historyColumns) noexcept
        {
            samplesPerColumn = std::max (1, (int) std::lround (sampleRate / 60.0));
            history.store (std::clamp (historyColumns, 16, 2048), std::memory_order_relaxed);
            colPeak = 0.0f; colOut = 0.0f; colCount = 0; colGr = 0.0f; colLfGr = 0.0f;
        }

        int getHistoryColumns() const noexcept { return history.load (std::memory_order_relaxed); }

        // Audio thread, once per base-rate sample.
        void pushBaseRateSample (float rawL, float rawR, float outL, float outR,
                                 float inGainDb, float ceilDb, const LimiterTelemetry& t) noexcept
        {
            colPeak = std::max (colPeak, std::max (std::fabs (rawL), std::fabs (rawR)));
            colOut  = std::max (colOut,  std::max (std::fabs (outL), std::fabs (outR)));
            colGr   = std::max (colGr, t.gainReductionDb);
            colLfGr = std::max (colLfGr, t.lfGainReductionDb);

            if (++colCount < samplesPerColumn)
                return;

            ScopeFrame f;
            f.inputDb  = (colPeak > 1.0e-6f ? 20.0f * std::log10 (colPeak) : -120.0f) + inGainDb;
            f.grDb     = colGr;
            f.lfGrDb   = colLfGr;
            f.ceilDb   = ceilDb;
            f.outputDb = colOut > 1.0e-6f ? 20.0f * std::log10 (colOut) : -120.0f;   // measured output peak

            const auto scope = fifo.write (1);
            if (scope.blockSize1 > 0)      frames[(size_t) scope.startIndex1] = f;
            else if (scope.blockSize2 > 0) frames[(size_t) scope.startIndex2] = f;
            // full FIFO (editor closed): frame dropped, never blocks

            colPeak = 0.0f; colOut = 0.0f; colCount = 0; colGr = 0.0f; colLfGr = 0.0f;
        }

        // Message thread. Returns number of frames copied.
        int pull (ScopeFrame* dest, int maxFrames) noexcept
        {
            const auto scope = fifo.read (std::min (maxFrames, fifo.getNumReady()));
            int n = 0;
            for (int i = 0; i < scope.blockSize1; ++i) dest[n++] = frames[(size_t) (scope.startIndex1 + i)];
            for (int i = 0; i < scope.blockSize2; ++i) dest[n++] = frames[(size_t) (scope.startIndex2 + i)];
            return n;
        }

    private:
        juce::AbstractFifo fifo { kFifoSize };
        std::array<ScopeFrame, kFifoSize> frames {};
        std::atomic<int> history { 180 };
        int samplesPerColumn = 800, colCount = 0;
        float colPeak = 0.0f, colOut = 0.0f, colGr = 0.0f, colLfGr = 0.0f;
    };

    // Live scope drawn inside the faceplate's glass screen.
    class OxfordHistoryScope final : public juce::Component, private juce::Timer
    {
    public:
        static constexpr int kMaxColumns = 600;   // 10 s at 60 Hz

        explicit OxfordHistoryScope (ScopeDataCollector& c) : collector (c)
        {
            frames.assign ((size_t) kMaxColumns, ScopeFrame {});
            setInterceptsMouseClicks (false, false);
            startTimerHz (60);
        }

        ~OxfordHistoryScope() override { stopTimer(); }

        void setCeilingDb (float db) noexcept         { ceilingDb = db; }
        void setRangeDb (float db) noexcept           { rangeDb = juce::jlimit (12.0f, 60.0f, db); }
        void setTimeSeconds (float sec) noexcept      { columns = juce::jlimit (60, kMaxColumns, (int) std::lround (sec * 60.0f)); }
        const ScopeFrame& getLatestFrame() const noexcept { return frames[(size_t) ((writeIdx + kMaxColumns - 1) % kMaxColumns)]; }

        void paint (juce::Graphics& g) override
        {
            const auto full = getLocalBounds().toFloat();
            const float u = full.getWidth() / 636.0f;                 // faceplate px -> screen px
            juce::Path glass; glass.addRoundedRectangle (full, 12.0f * u);
            g.reduceClipRegion (glass);

            g.setGradientFill (juce::ColourGradient (juce::Colour (0xFF041322), 0.0f, 0.0f,
                                                     juce::Colour (0xFF0A1E33), 0.0f, full.getBottom(), false));
            g.fillAll();

            auto area = full.reduced (6.0f * u, 6.0f * u);
            auto levelLane = area.removeFromTop (area.getHeight() * 0.66f);
            area.removeFromTop (3.0f * u);
            auto grLane = area;

            const auto gridCol = juce::Colour (0x2E7DD3FC);
            const auto labelCol = juce::Colour (0xFF8FB3CF);
            const float levelTop = 3.0f, levelBottom = -rangeDb, grMax = 12.0f;
            auto yLevel = [&] (float db) { return juce::jmap (juce::jlimit (levelBottom, levelTop, db), levelTop, levelBottom, levelLane.getY(), levelLane.getBottom()); };
            auto yGr    = [&] (float gr) { return juce::jmap (juce::jlimit (0.0f, grMax, gr), 0.0f, grMax, grLane.getY(), grLane.getBottom()); };

            g.setFont (juce::FontOptions (11.5f * u));
            const float step = rangeDb > 36.0f ? 12.0f : 6.0f;
            for (float db = 0.0f; db > levelBottom + 1.0f; db -= step)
            {
                const float y = yLevel (db);
                g.setColour (gridCol); g.drawHorizontalLine ((int) y, levelLane.getX(), levelLane.getRight());
                g.setColour (labelCol); g.drawText (juce::String ((int) db), (int) (levelLane.getX() + 2 * u), (int) (y - 13 * u), (int) (30 * u), (int) (12 * u), juce::Justification::left);
            }
            g.setColour (juce::Colour (0x557DD3FC));
            g.drawHorizontalLine ((int) grLane.getY() - 1, grLane.getX(), grLane.getRight());
            for (float gr : { 3.0f, 6.0f, 9.0f })
            {
                const float y = yGr (gr);
                g.setColour (gridCol); g.drawHorizontalLine ((int) y, grLane.getX(), grLane.getRight());
                g.setColour (labelCol); g.drawText ("-" + juce::String ((int) gr), (int) (grLane.getX() + 2 * u), (int) (y - 13 * u), (int) (30 * u), (int) (12 * u), juce::Justification::left);
            }

            const int n = columns;
            const float dx = levelLane.getWidth() / (float) (n - 1);
            juce::Path inFill, inLine, outLine, grLine, lfLine;
            inFill.startNewSubPath (levelLane.getX(), levelLane.getBottom());
            for (int i = 0; i < n; ++i)
            {
                const auto& f = frames[(size_t) ((writeIdx - n + i + 2 * kMaxColumns) % kMaxColumns)];
                const float x = levelLane.getX() + dx * (float) i;
                const float yi = yLevel (f.inputDb);
                inFill.lineTo (x, yi);
                if (i == 0) { inLine.startNewSubPath (x, yi); outLine.startNewSubPath (x, yLevel (f.outputDb)); grLine.startNewSubPath (x, yGr (f.grDb)); lfLine.startNewSubPath (x, yGr (f.lfGrDb)); }
                else        { inLine.lineTo (x, yi);          outLine.lineTo (x, yLevel (f.outputDb));          grLine.lineTo (x, yGr (f.grDb));          lfLine.lineTo (x, yGr (f.lfGrDb)); }
            }
            inFill.lineTo (levelLane.getRight(), levelLane.getBottom());
            inFill.closeSubPath();

            g.setGradientFill (juce::ColourGradient (juce::Colour (0x8C38BDF8), 0.0f, levelLane.getY(),
                                                     juce::Colour (0x1438BDF8), 0.0f, levelLane.getBottom(), false));
            g.fillPath (inFill);
            g.setColour (juce::Colour (0x5538BDF8)); g.strokePath (inLine, juce::PathStrokeType (3.5f * u));
            g.setColour (juce::Colour (0xFFA5E8FF)); g.strokePath (inLine, juce::PathStrokeType (1.1f * u));
            g.setColour (juce::Colour (0xD9F8FAFC)); g.strokePath (outLine, juce::PathStrokeType (1.4f * u));

            g.setColour (juce::Colour (0x44FBBF24)); g.strokePath (grLine, juce::PathStrokeType (5.0f * u));
            g.setColour (juce::Colour (0xFFFDE68A)); g.strokePath (grLine, juce::PathStrokeType (1.8f * u));
            g.setColour (juce::Colour (0xFF22D3EE)); g.strokePath (lfLine, juce::PathStrokeType (1.4f * u));

            {   // ceiling
                const float y = yLevel (ceilingDb);
                const float dashes[] = { 7.0f * u, 5.0f * u };
                g.setColour (juce::Colour (0xFFFF6B6B).withAlpha (0.85f));
                g.drawDashedLine ({ levelLane.getX(), y, levelLane.getRight(), y }, dashes, 2, 1.3f * u);
                g.setFont (juce::FontOptions (12.5f * u));
                g.drawText ("CEIL " + juce::String (ceilingDb, 2), (int) (levelLane.getRight() - 100 * u), (int) (y - 15 * u), (int) (96 * u), (int) (13 * u), juce::Justification::right);
            }

            g.setFont (juce::FontOptions (12.0f * u, juce::Font::bold));
            g.setColour (juce::Colour (0xFFFDE68A)); g.drawText ("GR", (int) (grLane.getRight() - 72 * u), (int) (grLane.getBottom() - 16 * u), (int) (30 * u), (int) (13 * u), juce::Justification::right);
            g.setColour (juce::Colour (0xFF22D3EE)); g.drawText ("LF", (int) (grLane.getRight() - 36 * u), (int) (grLane.getBottom() - 16 * u), (int) (30 * u), (int) (13 * u), juce::Justification::right);

            // glass: top sheen + edge vignette
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), 0.0f, full.getY(),
                                                     juce::Colours::transparentWhite, 0.0f, full.getY() + full.getHeight() * 0.35f, false));
            g.fillRect (full);
            g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, full.getCentreX(), full.getCentreY(),
                                                     juce::Colours::black.withAlpha (0.45f), full.getX(), full.getY(), true));
            g.fillRect (full);
        }

    private:
        void timerCallback() override
        {
            std::array<ScopeFrame, ScopeDataCollector::kFifoSize> tmp;
            const int got = collector.pull (tmp.data(), (int) tmp.size());
            for (int i = 0; i < got; ++i)
            {
                frames[(size_t) writeIdx] = tmp[(size_t) i];
                writeIdx = (writeIdx + 1) % kMaxColumns;
            }
            repaint();
        }

        ScopeDataCollector& collector;
        std::vector<ScopeFrame> frames;
        int writeIdx = 0, columns = 180;
        float ceilingDb = -0.1f, rangeDb = 36.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OxfordHistoryScope)
    };

    // Loudness read-out + 3 LED bar meters drawn into the faceplate's HUD window.
    class OxfordLoudnessHud final : public juce::Component
    {
    public:
        OxfordLoudnessHud() { setInterceptsMouseClicks (false, false); }

        void update (const LoudnessTelemetry& t, const ScopeFrame& f, float ceilDb) noexcept
        {
            loud = t; ceilingDb = ceilDb;
            outDb = std::max (f.outputDb, outDb - 1.5f);   // ~45 dB/s fall at 30 Hz
            grDb  = std::max (f.grDb,     grDb  - 0.6f);
            lfDb  = std::max (f.lfGrDb,   lfDb  - 0.6f);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const float u = (float) getWidth() / 242.0f;            // faceplate px -> screen px
            auto P = [u] (float x, float y, float w, float h) { return juce::Rectangle<float> (x * u, y * u, w * u, h * u); };
            auto fmt = [] (float v) { return v <= -69.9f ? juce::String ("--.-") : juce::String (v, 1); };

            g.setColour (juce::Colour (0xFFCBD5E1));
            g.setFont (juce::FontOptions (14.0f * u));
            g.drawText ("SHORT-TERM", P (8, 14, 160, 18), juce::Justification::left);

            const auto big = fmt (loud.outputShortTermLufs) + " LUFS";
            g.setFont (juce::FontOptions (35.0f * u, juce::Font::bold));
            g.setColour (juce::Colour (0x4022D3EE));
            for (auto o : { juce::Point<float> (-1.2f, 0), { 1.2f, 0 }, { 0, -1.2f }, { 0, 1.2f } })
                g.drawText (big, P (8, 38, 172, 42).translated (o.x * u, o.y * u), juce::Justification::left);
            g.setColour (juce::Colour (0xFF7DEBFF));
            g.drawText (big, P (8, 38, 172, 42), juce::Justification::left);

            struct Row { const char* label; juce::String value; juce::Colour c; };
            const auto plain = juce::Colour (0xFFE2E8F0);
            const bool over = loud.truePeakDb > ceilingDb + 0.05f;
            const Row rows[] = {
                { "INPUT S",    fmt (loud.inputShortTermLufs),   plain },
                { "MOMENTARY",  fmt (loud.outputMomentaryLufs),  plain },
                { "INTEGRATED", fmt (loud.outputIntegratedLufs), plain },
                { "TRUE PEAK",  loud.truePeakDb <= -119.0f ? juce::String ("-inf") : juce::String (loud.truePeakDb, 2),
                                over ? juce::Colour (0xFFFF6B6B) : juce::Colour (0xFF5EEAD4) },
                { "PLR",        juce::String (loud.plrDb, 1),    juce::Colour (0xFFFDE68A) },
                { "1:1 MATCH",  loud.matchEnabled ? juce::String (loud.matchGainDb, 1) : juce::String ("OFF"),
                                loud.matchEnabled ? juce::Colour (0xFF7DD3FC) : juce::Colour (0xFF64748B) } };

            float y = 88.0f;
            for (const auto& r : rows)
            {
                g.setFont (juce::FontOptions (14.5f * u));
                g.setColour (juce::Colour (0xFFCBD5E1));
                g.drawText (r.label, P (8, y, 100, 20), juce::Justification::left);
                g.setFont (juce::FontOptions (14.5f * u, juce::Font::bold));
                g.setColour (r.c);
                g.drawText (r.value, P (96, y, 76, 20), juce::Justification::right);
                y += 25.5f;
            }

            // LED bars: OUT (dBFS, -48..0), GR and LF (dB reduction, 0..12)
            drawBar (g, P (180, 11, 15, 218), juce::jmap (juce::jlimit (-48.0f, 0.0f, outDb), -48.0f, 0.0f, 0.0f, 1.0f), false);
            drawBar (g, P (197, 11, 15, 218), juce::jlimit (0.0f, 1.0f, grDb / 12.0f), true);
            drawBar (g, P (214, 11, 15, 218), juce::jlimit (0.0f, 1.0f, lfDb / 12.0f), true);

            g.setFont (juce::FontOptions (10.5f * u, juce::Font::bold));
            g.setColour (juce::Colour (0xFFFDE68A)); g.drawText ("GR",  P (192, 232, 25, 12), juce::Justification::centred);
            g.setColour (juce::Colour (0xFF22D3EE)); g.drawText ("LF",  P (209, 232, 25, 12), juce::Justification::centred);
        }

    private:
        static void drawBar (juce::Graphics& g, juce::Rectangle<float> r, float level, bool reduction)
        {
            constexpr int segs = 26;
            const float h = r.getHeight() / segs;
            for (int i = 0; i < segs; ++i)
            {
                const float frac = ((float) i + 0.5f) / (float) segs;                 // 0 = bottom
                auto seg = juce::Rectangle<float> (r.getX(), r.getBottom() - (float) (i + 1) * h, r.getWidth(), h).reduced (0.0f, h * 0.16f);
                juce::Colour on;
                if (reduction) on = frac > 0.75f ? juce::Colour (0xFFFF6B6B) : juce::Colour (0xFFFBBF24);
                else           on = frac > 0.92f ? juce::Colour (0xFFFF5A5A) : frac > 0.72f ? juce::Colour (0xFFFDE047) : juce::Colour (0xFF67E8F9);
                const bool lit = frac <= level;
                g.setColour (lit ? on : on.withAlpha (0.10f));
                g.fillRect (seg);
                if (lit) { g.setColour (on.withAlpha (0.25f)); g.fillRect (seg.expanded (1.0f, 0.5f)); }
            }
        }

        LoudnessTelemetry loud {};
        float ceilingDb = -0.1f, outDb = -120.0f, grDb = 0.0f, lfDb = 0.0f;
    };
} // namespace BubLeonDSP
