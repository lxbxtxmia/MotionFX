#pragma once
#include "Widgets.h"
#include <atomic>
#include <cmath>

namespace mfx
{
    class DriveBandPad final : public juce::Component,
                               public juce::SettableTooltipClient
    {
    public:
        DriveBandPad (
            juce::AudioProcessorValueTreeState& state,
            juce::String titleToUse,
            juce::String enableParameterId,
            juce::String gainParameterId,
            juce::String frequencyParameterId,
            juce::String bandwidthParameterId,
            juce::Colour accentToUse)
            : title (std::move (titleToUse)),
              accent (accentToUse)
        {
            enabledValue =
                state.getRawParameterValue (
                    enableParameterId);
            gainValue =
                state.getRawParameterValue (
                    gainParameterId);
            frequencyValue =
                state.getRawParameterValue (
                    frequencyParameterId);
            bandwidthValue =
                state.getRawParameterValue (
                    bandwidthParameterId);

            enabledParameter =
                state.getParameter (
                    enableParameterId);
            gainParameter =
                state.getParameter (
                    gainParameterId);
            frequencyParameter =
                state.getParameter (
                    frequencyParameterId);
            bandwidthParameter =
                state.getParameter (
                    bandwidthParameterId);

            setTooltip (
                title
                + ": drag horizontally for frequency, "
                  "vertically for gain, and Alt/Option-drag "
                  "vertically for bandwidth.");
            setMouseCursor (
                juce::MouseCursor::CrosshairCursor);
        }

        void paint (
            juce::Graphics& graphics) override
        {
            auto bounds =
                getLocalBounds().toFloat();
            auto titleArea =
                bounds.removeFromTop (22.0f);
            auto valueArea =
                bounds.removeFromBottom (22.0f);
            auto graph =
                bounds.reduced (5.0f, 3.0f);

            const bool enabled =
                enabledValue != nullptr
                && enabledValue->load (
                    std::memory_order_relaxed)
                    > 0.5f;

            graphics.setColour (
                enabled
                    ? accent
                    : Palette::textDim);
            graphics.setFont (
                FontBank::font (11.5f, true));
            graphics.drawText (
                title,
                titleArea.toNearestInt(),
                juce::Justification::centredLeft);

            graphics.setColour (
                Palette::panel.withAlpha (0.90f));
            graphics.fillRoundedRectangle (
                graph,
                5.0f);

            graphics.setColour (
                Palette::stroke.withAlpha (0.78f));
            graphics.drawRoundedRectangle (
                graph,
                5.0f,
                1.0f);

            graphics.setColour (
                Palette::stroke.withAlpha (0.32f));

            for (int index = 1;
                 index < 8;
                 ++index)
            {
                const float x =
                    graph.getX()
                    + graph.getWidth()
                        * (float) index
                        / 8.0f;
                graphics.drawVerticalLine (
                    (int) std::lround (x),
                    graph.getY(),
                    graph.getBottom());
            }

            for (int index = 1;
                 index < 4;
                 ++index)
            {
                const float y =
                    graph.getY()
                    + graph.getHeight()
                        * (float) index
                        / 4.0f;
                graphics.drawHorizontalLine (
                    (int) std::lround (y),
                    graph.getX(),
                    graph.getRight());
            }

            const float gain =
                readRaw (gainValue);
            const float frequency =
                readRaw (frequencyValue);
            const float bandwidth =
                readRaw (bandwidthValue);

            const float frequencyNormalised =
                frequencyParameter != nullptr
                    ? frequencyParameter
                        ->convertTo0to1 (
                            frequency)
                    : 0.5f;
            const float gainNormalised =
                gainParameter != nullptr
                    ? gainParameter
                        ->convertTo0to1 (
                            gain)
                    : 0.0f;
            const float bandwidthNormalised =
                bandwidthParameter != nullptr
                    ? bandwidthParameter
                        ->convertTo0to1 (
                            bandwidth)
                    : 0.5f;

            const auto point =
                juce::Point<float> (
                    graph.getX()
                        + frequencyNormalised
                            * graph.getWidth(),
                    graph.getBottom()
                        - gainNormalised
                            * graph.getHeight());

            juce::Path curve;
            const float spread =
                juce::jmap (
                    bandwidthNormalised,
                    0.0f,
                    1.0f,
                    0.045f,
                    0.34f);

            for (int index = 0;
                 index <= 96;
                 ++index)
            {
                const float normalisedX =
                    (float) index / 96.0f;
                const float distance =
                    normalisedX
                    - frequencyNormalised;
                const float gaussian =
                    std::exp (
                        -(distance * distance)
                        / juce::jmax (
                            0.0001f,
                            spread * spread));
                const float normalisedY =
                    0.50f
                    - gaussian
                        * gainNormalised
                        * 0.43f;
                const float x =
                    graph.getX()
                    + normalisedX
                        * graph.getWidth();
                const float y =
                    graph.getY()
                    + normalisedY
                        * graph.getHeight();

                if (index == 0)
                    curve.startNewSubPath (x, y);
                else
                    curve.lineTo (x, y);
            }

            graphics.setColour (
                accent.withAlpha (
                    enabled ? 0.96f : 0.34f));
            graphics.strokePath (
                curve,
                juce::PathStrokeType (
                    2.0f,
                    juce::PathStrokeType::curved,
                    juce::PathStrokeType::rounded));

            graphics.setColour (
                enabled
                    ? accent
                    : Palette::textDim);
            graphics.fillEllipse (
                point.x - 4.5f,
                point.y - 4.5f,
                9.0f,
                9.0f);

            graphics.setColour (
                Palette::text);
            graphics.setFont (
                FontBank::font (10.5f, true));

            const auto gainText =
                ValueFormatting::compactDecimal (
                    gain,
                    gain < 10.0f ? 1 : 0)
                + " dB";
            const auto frequencyText =
                ValueFormatting::frequencyHz (
                    frequency,
                    false);
            const auto bandwidthText =
                "B "
                + ValueFormatting::compactDecimal (
                    bandwidth,
                    2);

            auto first =
                valueArea.removeFromLeft (
                    (int) (valueArea.getWidth()
                           * 0.27f));
            auto last =
                valueArea.removeFromRight (
                    (int) (valueArea.getWidth()
                           * 0.25f));

            graphics.drawText (
                gainText,
                first.toNearestInt(),
                juce::Justification::centredLeft);
            graphics.drawText (
                frequencyText,
                valueArea.toNearestInt(),
                juce::Justification::centred);
            graphics.drawText (
                bandwidthText,
                last.toNearestInt(),
                juce::Justification::centredRight);
        }

        void mouseDown (
            const juce::MouseEvent& event) override
        {
            beginGestures();
            dragStart = event.position;
            startBandwidthNormalised =
                bandwidthParameter != nullptr
                    ? bandwidthParameter->getValue()
                    : 0.5f;
            updateFromEvent (event);
        }

        void mouseDrag (
            const juce::MouseEvent& event) override
        {
            updateFromEvent (event);
        }

        void mouseUp (
            const juce::MouseEvent&) override
        {
            endGestures();
        }

        void mouseDoubleClick (
            const juce::MouseEvent&) override
        {
            resetParameter (gainParameter);
            resetParameter (frequencyParameter);
            resetParameter (bandwidthParameter);
            repaint();
        }

    private:
        static float readRaw (
            std::atomic<float>* value) noexcept
        {
            return value != nullptr
                ? value->load (
                    std::memory_order_relaxed)
                : 0.0f;
        }

        static void resetParameter (
            juce::RangedAudioParameter* parameter)
        {
            if (parameter == nullptr)
                return;

            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (
                parameter->getDefaultValue());
            parameter->endChangeGesture();
        }

        void beginGestures()
        {
            for (auto* parameter : {
                     gainParameter,
                     frequencyParameter,
                     bandwidthParameter
                 })
            {
                if (parameter != nullptr)
                    parameter->beginChangeGesture();
            }
        }

        void endGestures()
        {
            for (auto* parameter : {
                     gainParameter,
                     frequencyParameter,
                     bandwidthParameter
                 })
            {
                if (parameter != nullptr)
                    parameter->endChangeGesture();
            }
        }

        void updateFromEvent (
            const juce::MouseEvent& event)
        {
            auto graph =
                getLocalBounds()
                    .toFloat()
                    .withTrimmedTop (22.0f)
                    .withTrimmedBottom (22.0f)
                    .reduced (5.0f, 3.0f);

            if (event.mods.isAltDown())
            {
                if (bandwidthParameter != nullptr)
                {
                    const float delta =
                        (dragStart.y
                         - event.position.y)
                        / juce::jmax (
                            1.0f,
                            graph.getHeight());

                    bandwidthParameter
                        ->setValueNotifyingHost (
                            juce::jlimit (
                                0.0f,
                                1.0f,
                                startBandwidthNormalised
                                    + delta));
                }
            }
            else
            {
                if (frequencyParameter != nullptr)
                {
                    frequencyParameter
                        ->setValueNotifyingHost (
                            juce::jlimit (
                                0.0f,
                                1.0f,
                                (event.position.x
                                 - graph.getX())
                                / juce::jmax (
                                    1.0f,
                                    graph.getWidth())));
                }

                if (gainParameter != nullptr)
                {
                    gainParameter
                        ->setValueNotifyingHost (
                            juce::jlimit (
                                0.0f,
                                1.0f,
                                (graph.getBottom()
                                 - event.position.y)
                                / juce::jmax (
                                    1.0f,
                                    graph.getHeight())));
                }
            }

            repaint();
        }

        juce::String title;
        juce::Colour accent;

        std::atomic<float>* enabledValue = nullptr;
        std::atomic<float>* gainValue = nullptr;
        std::atomic<float>* frequencyValue = nullptr;
        std::atomic<float>* bandwidthValue = nullptr;

        juce::RangedAudioParameter*
            enabledParameter = nullptr;
        juce::RangedAudioParameter*
            gainParameter = nullptr;
        juce::RangedAudioParameter*
            frequencyParameter = nullptr;
        juce::RangedAudioParameter*
            bandwidthParameter = nullptr;

        juce::Point<float> dragStart;
        float startBandwidthNormalised = 0.5f;
    };

    class DrivePhasePanel final : public juce::Component,
                                  private juce::Timer
    {
    public:
        explicit DrivePhasePanel (
            juce::AudioProcessorValueTreeState& state)
            : tracePad (
                  state,
                  "TRACING MODEL",
                  "drive_trace_enabled",
                  "drive_trace_gain",
                  "drive_trace_freq",
                  "drive_trace_bandwidth",
                  Palette::orange),
              pinchPad (
                  state,
                  "PINCH",
                  "drive_pinch_enabled",
                  "drive_pinch_gain",
                  "drive_pinch_freq",
                  "drive_pinch_bandwidth",
                  Palette::pink),
              traceEnabled (
                  state,
                  "drive_trace_enabled",
                  "TRACE ON"),
              pinchEnabled (
                  state,
                  "drive_pinch_enabled",
                  "PINCH ON"),
              character (
                  state,
                  "drive_groove_character",
                  "CHARACTER"),
              stereoPinch (
                  state,
                  "drive_pinch_stereo",
                  "STEREO PINCH")
        {
            addAndMakeVisible (tracePad);
            addAndMakeVisible (pinchPad);
            addAndMakeVisible (traceEnabled);
            addAndMakeVisible (pinchEnabled);
            addAndMakeVisible (character);
            addAndMakeVisible (stereoPinch);

            character.combo.setTooltip (
                "Soft gives smoother dubplate-like phase distortion. "
                "Hard uses sharper tracing and pinch curves.");
            stereoPinch.button.setTooltip (
                "Invert the Pinch component on the right channel for a wider 180-degree stereo image.");

            startTimerHz (20);
        }

        void resized() override
        {
            auto bounds =
                getLocalBounds().reduced (5);
            auto controls =
                bounds.removeFromTop (52);

            const int gap = 8;
            const int itemWidth =
                (controls.getWidth()
                 - gap * 3)
                / 4;

            traceEnabled.setBounds (
                controls.removeFromLeft (
                    itemWidth));
            controls.removeFromLeft (gap);

            pinchEnabled.setBounds (
                controls.removeFromLeft (
                    itemWidth));
            controls.removeFromLeft (gap);

            character.setBounds (
                controls.removeFromLeft (
                    itemWidth));
            controls.removeFromLeft (gap);

            stereoPinch.setBounds (controls);

            bounds.removeFromTop (5);
            const int padWidth =
                (bounds.getWidth() - 10) / 2;

            tracePad.setBounds (
                bounds.removeFromLeft (
                    padWidth));
            bounds.removeFromLeft (10);
            pinchPad.setBounds (bounds);
        }

    private:
        void timerCallback() override
        {
            tracePad.repaint();
            pinchPad.repaint();
        }

        DriveBandPad tracePad;
        DriveBandPad pinchPad;
        LabeledToggle traceEnabled;
        LabeledToggle pinchEnabled;
        LabeledCombo character;
        LabeledToggle stereoPinch;
    };
}
