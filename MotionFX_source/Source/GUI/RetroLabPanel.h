#pragma once
#include "Widgets.h"
#include "../DSP/RetroEffect.h"
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace mfx
{
    class RetroLabPanel final : public juce::Component,
                                private juce::Timer
    {
    public:
        RetroLabPanel (juce::AudioProcessorValueTreeState& state,
                       RetroEffect& effect)
            : apvts (state), retro (effect)
        {
            bitHold = std::make_unique<LabeledCombo> (
                apvts, "retro_bit_hold", "HOLD");
            bitDither = std::make_unique<LabeledToggle> (
                apvts, "retro_bit_dither", "DITHER");
            bitAntiAlias = std::make_unique<LabeledToggle> (
                apvts, "retro_bit_antialias", "ANTI-ALIAS");

            lossyQuality = std::make_unique<LabeledCombo> (
                apvts, "retro_lossy_quality", "QUALITY");
            lossyStereoLink = std::make_unique<LabeledToggle> (
                apvts, "retro_lossy_stereo_link", "STEREO LINK");

            spFilter = std::make_unique<LabeledCombo> (
                apvts, "retro_sp_filter", "OUTPUT FILTER");

            tapeMachine = std::make_unique<LabeledCombo> (
                apvts, "retro_tape_machine", "MACHINE");
            tapeSpeed = std::make_unique<LabeledCombo> (
                apvts, "retro_tape_speed", "SPEED");
            tapeNoiseReduction = std::make_unique<LabeledCombo> (
                apvts, "retro_tape_nr", "NOISE REDUCTION");

            addControl (*bitHold);
            addControl (*bitDither);
            addControl (*bitAntiAlias);
            addControl (*lossyQuality);
            addControl (*lossyStereoLink);
            addControl (*spFilter);
            addControl (*tapeMachine);
            addControl (*tapeSpeed);
            addControl (*tapeNoiseReduction);

            bitHold->combo.setTooltip (
                "Step holds each reduced-rate sample. Linear and Smooth interpolate between held samples.");
            bitDither->button.setTooltip (
                "Adds low-level triangular dither before bit-depth quantisation.");
            bitAntiAlias->button.setTooltip (
                "Low-passes before sample-rate reduction to reduce fold-back aliasing.");
            lossyQuality->combo.setTooltip (
                "Eco uses 256-point frames, Normal 512 and High 1024. Larger frames improve spectral resolution and add latency.");
            lossyStereoLink->button.setTooltip (
                "Uses the same spectral damage pattern on both channels.");
            spFilter->combo.setTooltip (
                "SP-style output choices: unfiltered, fixed low-pass or level-dependent dynamic low-pass.");
            tapeMachine->combo.setTooltip (
                "Reel-to-Reel and Cassette use different motion, head-loss and noise profiles.");
            tapeSpeed->combo.setTooltip (
                "Tape speed changes bandwidth, head bump and mechanical instability.");
            tapeNoiseReduction->combo.setTooltip (
                "Off disables the combined NR/Denoise path. Type B is gentler; Type C applies a stronger extended response.");

            lastMode = -1;
            refreshVisibility();
            startTimerHz (24);
        }

        void paint (juce::Graphics& graphics) override
        {
            auto bounds = getLocalBounds().toFloat();
            graphics.setColour (Palette::bg1);
            graphics.fillRoundedRectangle (bounds, 6.0f);
            graphics.setColour (Palette::stroke);
            graphics.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

            auto graph = bounds.reduced (8.0f);
            if (hasVisibleControls())
                graph.removeFromTop (54.0f);

            graphics.setColour (Palette::stroke.withAlpha (0.34f));
            for (int index = 1; index < 8; ++index)
            {
                const float x = graph.getX()
                    + graph.getWidth() * (float) index / 8.0f;
                graphics.drawVerticalLine (
                    (int) std::lround (x), graph.getY(), graph.getBottom());
            }
            for (int index = 1; index < 4; ++index)
            {
                const float y = graph.getY()
                    + graph.getHeight() * (float) index / 4.0f;
                graphics.drawHorizontalLine (
                    (int) std::lround (y), graph.getX(), graph.getRight());
            }

            const int mode = getMode();
            switch (mode)
            {
                case 0: drawBitcrush (graphics, graph); break;
                case 1: drawLossy (graphics, graph); break;
                case 2: drawWear (graphics, graph, false); break;
                case 3: drawSp (graphics, graph); break;
                case 4: drawWear (graphics, graph, true); break;
                case 5: drawVinyl (graphics, graph); break;
                default: break;
            }
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (8);
            auto controls = bounds.removeFromTop (50);

            std::vector<juce::Component*> visible;
            for (auto* component : {
                     (juce::Component*) bitHold.get(),
                     (juce::Component*) bitDither.get(),
                     (juce::Component*) bitAntiAlias.get(),
                     (juce::Component*) lossyQuality.get(),
                     (juce::Component*) lossyStereoLink.get(),
                     (juce::Component*) spFilter.get(),
                     (juce::Component*) tapeMachine.get(),
                     (juce::Component*) tapeSpeed.get(),
                     (juce::Component*) tapeNoiseReduction.get()
                 })
            {
                if (component != nullptr && component->isVisible())
                    visible.push_back (component);
            }

            if (visible.empty())
                return;

            constexpr int gap = 8;
            const int width = juce::jmax (
                96,
                (controls.getWidth() - gap * ((int) visible.size() - 1))
                    / (int) visible.size());

            for (size_t index = 0; index < visible.size(); ++index)
            {
                const bool last = index + 1 == visible.size();
                visible[index]->setBounds (
                    last ? controls : controls.removeFromLeft (width));
                if (! last)
                    controls.removeFromLeft (gap);
            }
        }

    private:
        template <typename Control>
        void addControl (Control& control)
        {
            addAndMakeVisible (control);
        }

        float raw (const juce::String& parameterId,
                   float fallback = 0.0f) const noexcept
        {
            if (auto* value = apvts.getRawParameterValue (parameterId))
                return value->load (std::memory_order_relaxed);
            return fallback;
        }

        int getMode() const noexcept
        {
            return (int) raw ("retro_mode", 0.0f);
        }

        bool hasVisibleControls() const noexcept
        {
            for (auto* component : {
                     (juce::Component*) bitHold.get(),
                     (juce::Component*) bitDither.get(),
                     (juce::Component*) bitAntiAlias.get(),
                     (juce::Component*) lossyQuality.get(),
                     (juce::Component*) lossyStereoLink.get(),
                     (juce::Component*) spFilter.get(),
                     (juce::Component*) tapeMachine.get(),
                     (juce::Component*) tapeSpeed.get(),
                     (juce::Component*) tapeNoiseReduction.get()
                 })
            {
                if (component != nullptr && component->isVisible())
                    return true;
            }
            return false;
        }

        void refreshVisibility()
        {
            const int mode = getMode();
            if (mode == lastMode)
                return;

            bitHold->setVisible (mode == 0);
            bitDither->setVisible (mode == 0);
            bitAntiAlias->setVisible (mode == 0);
            lossyQuality->setVisible (mode == 1);
            lossyStereoLink->setVisible (mode == 1);
            spFilter->setVisible (mode == 3);
            tapeMachine->setVisible (mode == 4);
            tapeSpeed->setVisible (mode == 4);
            tapeNoiseReduction->setVisible (mode == 4);

            lastMode = mode;
            resized();
            repaint();
        }

        static juce::Point<float> mapPoint (
            juce::Rectangle<float> area,
            float xNormalised,
            float yNormalised) noexcept
        {
            return {
                area.getX() + juce::jlimit (0.0f, 1.0f, xNormalised) * area.getWidth(),
                area.getBottom() - juce::jlimit (0.0f, 1.0f, yNormalised) * area.getHeight()
            };
        }

        void drawModeLabel (juce::Graphics& graphics,
                            juce::Rectangle<float> area,
                            const juce::String& text,
                            juce::Colour colour) const
        {
            graphics.setFont (FontBank::font (11.5f, true));
            graphics.setColour (colour);
            graphics.drawText (
                text,
                area.removeFromTop (18.0f).toNearestInt(),
                juce::Justification::topLeft);
        }

        void drawBitcrush (juce::Graphics& graphics,
                           juce::Rectangle<float> area)
        {
            drawModeLabel (graphics, area, "QUANTISATION / SAMPLE HOLD", Palette::pink);
            area = area.reduced (5.0f, 20.0f);
            const int bits = (int) raw ("retro_bits", 12.0f);
            const int visibleSteps = juce::jlimit (4, 32, 2 + bits * 2);
            juce::Path staircase;

            for (int index = 0; index <= visibleSteps; ++index)
            {
                const float x = (float) index / (float) visibleSteps;
                const float sine = 0.5f + 0.38f * std::sin (
                    x * juce::MathConstants<float>::twoPi);
                const float levels = (float) juce::jlimit (4, 64, 1 << juce::jmin (6, bits));
                const float quantised = std::round (sine * levels) / levels;
                const auto point = mapPoint (area, x, quantised);

                if (index == 0)
                    staircase.startNewSubPath (point);
                else
                {
                    const auto previous = staircase.getCurrentPosition();
                    staircase.lineTo (point.x, previous.y);
                    staircase.lineTo (point);
                }
            }

            graphics.setColour (Palette::pink);
            graphics.strokePath (
                staircase,
                juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
            graphics.setColour (Palette::textDim);
            graphics.setFont (FontBank::font (10.5f));
            graphics.drawText (
                juce::String (bits) + " BIT  ·  "
                    + ValueFormatting::frequencyHz (
                        raw ("retro_sample_rate", 12000.0f), false),
                area.toNearestInt(),
                juce::Justification::bottomRight);
        }

        void drawLossy (
            juce::Graphics& graphics,
            juce::Rectangle<float> area)
        {
            drawModeLabel (
                graphics,
                area,
                "SPECTRAL RANGE / BIN SCRAMBLE",
                Palette::purple);
            area = area.reduced (
                4.0f,
                18.0f);

            const float range =
                raw (
                    "retro_lossy_bandwidth",
                    12000.0f);
            const float logarithmicRange =
                std::log2 (
                    juce::jmax (
                        20.0f,
                        range)
                    / 20.0f)
                / std::log2 (
                    24000.0f
                    / 20.0f);
            const float rangeX =
                area.getX()
                + juce::jlimit (
                    0.0f,
                    1.0f,
                    logarithmicRange)
                    * area.getWidth();

            graphics.setColour (
                Palette::purple.withAlpha (
                    0.075f));
            graphics.fillRect (
                area.withRight (
                    rangeX));

            juce::Path inputPath;
            juce::Path outputPath;

            for (int band = 0;
                 band < RetroEffect::spectrumBands;
                 ++band)
            {
                const float x =
                    (float) band
                    / (float) (
                        RetroEffect::spectrumBands
                        - 1);
                const float input =
                    retro.uiInputSpectrum[
                        (size_t) band].load (
                            std::memory_order_relaxed);
                const float output =
                    retro.uiOutputSpectrum[
                        (size_t) band].load (
                            std::memory_order_relaxed);
                const auto inputPoint =
                    mapPoint (
                        area,
                        x,
                        input * 0.92f);
                const auto outputPoint =
                    mapPoint (
                        area,
                        x,
                        output * 0.92f);

                if (band == 0)
                {
                    inputPath.startNewSubPath (
                        inputPoint);
                    outputPath.startNewSubPath (
                        outputPoint);
                }
                else
                {
                    inputPath.lineTo (
                        inputPoint);
                    outputPath.lineTo (
                        outputPoint);
                }
            }

            graphics.setColour (
                Palette::textDim.withAlpha (
                    0.55f));
            graphics.strokePath (
                inputPath,
                juce::PathStrokeType (
                    1.2f));

            graphics.setColour (
                Palette::purple);
            graphics.strokePath (
                outputPath,
                juce::PathStrokeType (
                    2.0f,
                    juce::PathStrokeType::curved,
                    juce::PathStrokeType::rounded));

            graphics.setColour (
                Palette::purple.withAlpha (
                    0.72f));
            graphics.drawVerticalLine (
                (int) std::lround (
                    rangeX),
                area.getY(),
                area.getBottom());

            graphics.setColour (
                Palette::textDim);
            graphics.setFont (
                FontBank::font (
                    10.5f));
            graphics.drawText (
                "RANGE "
                    + ValueFormatting::frequencyHz (
                        range,
                        false)
                    + "  ·  SCRAMBLE "
                    + ValueFormatting::percent (
                        raw (
                            "retro_lossy_scramble",
                            35.0f),
                        false),
                area.toNearestInt(),
                juce::Justification::bottomRight);
        }

        void drawWear (juce::Graphics& graphics,
                       juce::Rectangle<float> area,
                       bool tape)
        {
            drawModeLabel (
                graphics,
                area,
                tape ? "TAPE MOTION / HEAD RESPONSE" : "WOW / FLUTTER / DROPOUT",
                tape ? Palette::orange : Palette::teal);
            area = area.reduced (5.0f, 20.0f);

            const float liveMotion = retro.uiMotion.load (std::memory_order_relaxed);
            const float dropout = retro.uiDropout.load (std::memory_order_relaxed);
            juce::Path motionPath;
            const float speed = tape
                ? RetroEffect::tapeSpeedIps ((TapeSpeed) (int) raw ("retro_tape_speed", 3.0f))
                : 7.5f;

            for (int index = 0; index <= 120; ++index)
            {
                const float x = (float) index / 120.0f;
                const float phase = x * juce::MathConstants<float>::twoPi;
                const float wave = 0.5f
                    + 0.23f * std::sin (phase * (tape ? 1.0f : 1.6f) + liveMotion)
                    + 0.07f * std::sin (phase * (tape ? 8.0f : 12.0f));
                const auto point = mapPoint (area, x, wave);
                if (index == 0)
                    motionPath.startNewSubPath (point);
                else
                    motionPath.lineTo (point);
            }

            graphics.setColour (tape ? Palette::orange : Palette::teal);
            graphics.strokePath (
                motionPath,
                juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

            if (! tape)
            {
                graphics.setColour (
                    Palette::pink.withAlpha (
                        juce::jlimit (
                            0.0f,
                            0.52f,
                            dropout * 0.52f)));
                graphics.fillRoundedRectangle (
                    area,
                    4.0f);
            }
            else
            {
                graphics.setColour (Palette::textDim);
                graphics.setFont (FontBank::font (10.5f));
                graphics.drawText (
                    ValueFormatting::compactDecimal (speed, speed < 10.0f ? 3 : 0)
                        + " IPS",
                    area.toNearestInt(),
                    juce::Justification::bottomRight);
            }
        }

        void drawSp (juce::Graphics& graphics,
                     juce::Rectangle<float> area)
        {
            drawModeLabel (graphics, area, "12-BIT CLOCK / OUTPUT FILTER", Palette::orange);
            area = area.reduced (5.0f, 20.0f);
            juce::Path response;
            const float cutoff = raw ("retro_sp_filter_cutoff", 8500.0f);
            const int filterMode = (int) raw ("retro_sp_filter", 1.0f);

            for (int index = 0; index <= 100; ++index)
            {
                const float x = (float) index / 100.0f;
                const float frequency = 20.0f * std::pow (1000.0f, x);
                float amplitude = 0.88f;
                if (filterMode != 0)
                {
                    const float ratio = frequency / juce::jmax (20.0f, cutoff);
                    amplitude /= std::sqrt (1.0f + std::pow (ratio, 8.0f));
                }
                const auto point = mapPoint (area, x, amplitude);
                if (index == 0)
                    response.startNewSubPath (point);
                else
                    response.lineTo (point);
            }

            graphics.setColour (Palette::orange);
            graphics.strokePath (
                response,
                juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
            graphics.setColour (Palette::textDim);
            graphics.setFont (FontBank::font (10.5f));
            graphics.drawText (
                "12 BIT  ·  "
                    + ValueFormatting::frequencyHz (
                        retro.spEffectiveSampleRate(), false),
                area.toNearestInt(),
                juce::Justification::bottomRight);
        }

        void drawVinyl (juce::Graphics& graphics,
                        juce::Rectangle<float> area)
        {
            drawModeLabel (graphics, area, "SURFACE NOISE / DUST EVENTS", Palette::pink);
            area = area.reduced (5.0f, 20.0f);
            juce::Path noisePath;
            const float surface = raw ("retro_vinyl_surface", 18.0f) / 100.0f;
            const float crackle = raw ("retro_vinyl_crackle", 12.0f) / 100.0f;

            for (int index = 0; index <= 120; ++index)
            {
                const float x = (float) index / 120.0f;
                const float pseudo = std::sin ((float) index * 12.9898f) * 43758.5453f;
                const float noise = pseudo - std::floor (pseudo);
                float y = 0.5f + (noise - 0.5f) * surface * 0.55f;
                if (index % juce::jmax (5, 34 - (int) (crackle * 28.0f)) == 0)
                    y += (index % 2 == 0 ? 1.0f : -1.0f) * crackle * 0.38f;
                const auto point = mapPoint (area, x, y);
                if (index == 0)
                    noisePath.startNewSubPath (point);
                else
                    noisePath.lineTo (point);
            }

            graphics.setColour (Palette::pink);
            graphics.strokePath (
                noisePath,
                juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
        }

        void timerCallback() override
        {
            refreshVisibility();
            repaint();
        }

        juce::AudioProcessorValueTreeState& apvts;
        RetroEffect& retro;

        std::unique_ptr<LabeledCombo> bitHold;
        std::unique_ptr<LabeledToggle> bitDither;
        std::unique_ptr<LabeledToggle> bitAntiAlias;
        std::unique_ptr<LabeledCombo> lossyQuality;
        std::unique_ptr<LabeledToggle> lossyStereoLink;
        std::unique_ptr<LabeledCombo> spFilter;
        std::unique_ptr<LabeledCombo> tapeMachine;
        std::unique_ptr<LabeledCombo> tapeSpeed;
        std::unique_ptr<LabeledCombo> tapeNoiseReduction;
        int lastMode = -1;
    };
}
