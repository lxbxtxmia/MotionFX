#pragma once
#include "Widgets.h"
#include <atomic>
#include <cmath>

namespace mfx
{
    class EditableParameterLabel final : public juce::Label
    {
    public:
        enum class Unit
        {
            Decibels,
            Frequency,
            Bandwidth
        };

        EditableParameterLabel (
            juce::RangedAudioParameter* parameterToUse,
            std::atomic<float>* valueToUse,
            Unit unitToUse)
            : parameter (parameterToUse),
              value (valueToUse),
              unit (unitToUse)
        {
            getProperties().set ("mfxNumeric", true);
            setEditable (true, true, false);
            setJustificationType (juce::Justification::centred);
            setColour (juce::Label::textColourId, Palette::text);
            setColour (juce::Label::backgroundColourId,
                       juce::Colours::transparentBlack);
            setColour (juce::Label::outlineColourId,
                       juce::Colours::transparentBlack);
            setColour (juce::Label::textWhenEditingColourId,
                       Palette::text);
            setColour (juce::Label::backgroundWhenEditingColourId,
                       Palette::panelHi);
            setColour (juce::Label::outlineWhenEditingColourId,
                       Palette::teal);
            setFont (FontBank::numericFont (11.0f, true));
            setTooltip (
                "Click to type, or drag vertically to change the value. "
                "Hold Shift for fine adjustment. Double-click resets to default.");

            onTextChange = [this]
            {
                if (updatingText || parameter == nullptr)
                    return;

                const double parsed = parseText (getText());
                const auto range = parameter->getNormalisableRange();
                const float rawValue = (float) juce::jlimit (
                    (double) range.start,
                    (double) range.end,
                    parsed);

                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (
                    parameter->convertTo0to1 (rawValue));
                parameter->endChangeGesture();
            };

            refresh();
        }

        void refresh()
        {
            if (isBeingEdited() || value == nullptr)
                return;

            updatingText = true;
            setText (formatValue (
                         value->load (std::memory_order_relaxed)),
                     juce::dontSendNotification);
            updatingText = false;
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            dragStartY = event.position.y;
            dragStartNormalised = parameter != nullptr
                ? parameter->getValue()
                : 0.0f;
            dragChanged = false;
            dragGestureActive = false;

            // Label opens its editor on mouse-up when no drag occurred.
            juce::Label::mouseDown (event);
        }

        void mouseDrag (const juce::MouseEvent& event) override
        {
            if (parameter == nullptr)
                return;

            const float deltaY = dragStartY - event.position.y;

            if (! dragChanged && std::abs (deltaY) < 2.0f)
                return;

            if (! dragGestureActive)
            {
                dragChanged = true;
                dragGestureActive = true;
                parameter->beginChangeGesture();
                setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            }

            float pixelsForFullRange = 180.0f;

            switch (unit)
            {
                case Unit::Decibels:
                    pixelsForFullRange = 190.0f;
                    break;
                case Unit::Frequency:
                    pixelsForFullRange = 250.0f;
                    break;
                case Unit::Bandwidth:
                    pixelsForFullRange = 170.0f;
                    break;
            }

            if (event.mods.isShiftDown())
                pixelsForFullRange *= 5.0f;

            const float normalised = juce::jlimit (
                0.0f,
                1.0f,
                dragStartNormalised + deltaY / pixelsForFullRange);

            parameter->setValueNotifyingHost (normalised);
            refresh();
        }

        void mouseUp (const juce::MouseEvent& event) override
        {
            if (dragGestureActive && parameter != nullptr)
                parameter->endChangeGesture();

            if (dragChanged)
            {
                dragGestureActive = false;
                dragChanged = false;
                setMouseCursor (juce::MouseCursor::NormalCursor);
                refresh();
                return;
            }

            juce::Label::mouseUp (event);
        }

        void mouseDoubleClick (
            const juce::MouseEvent& event) override
        {
            if (parameter != nullptr)
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (
                    parameter->getDefaultValue());
                parameter->endChangeGesture();
                refresh();
            }

            juce::ignoreUnused (event);
        }

    private:
        juce::String formatValue (float rawValue) const
        {
            switch (unit)
            {
                case Unit::Decibels:
                    return ValueFormatting::compactDecimal (
                               rawValue,
                               rawValue < 10.0f ? 1 : 0)
                        + " dB";

                case Unit::Frequency:
                    return ValueFormatting::frequencyHz (
                        rawValue,
                        false);

                case Unit::Bandwidth:
                    return "B "
                        + ValueFormatting::compactDecimal (
                            rawValue,
                            2);
            }

            return {};
        }

        double parseText (juce::String text) const
        {
            switch (unit)
            {
                case Unit::Decibels:
                    return text.getDoubleValue();

                case Unit::Frequency:
                    return ValueFormatting::parseEngineeringValue (
                        text);

                case Unit::Bandwidth:
                    return text
                        .removeCharacters ("Bb")
                        .trim()
                        .getDoubleValue();
            }

            return 0.0;
        }

        juce::RangedAudioParameter* parameter = nullptr;
        std::atomic<float>* value = nullptr;
        Unit unit = Unit::Bandwidth;
        bool updatingText = false;
        float dragStartY = 0.0f;
        float dragStartNormalised = 0.0f;
        bool dragChanged = false;
        bool dragGestureActive = false;
    };

    class DriveBandPad final : public juce::Component,
                               public juce::SettableTooltipClient
    {
    public:
        DriveBandPad (
            juce::AudioProcessorValueTreeState& state,
            DriveEffect& driveEffect,
            juce::String titleToUse,
            juce::String enableParameterId,
            juce::String gainParameterId,
            juce::String frequencyParameterId,
            juce::String bandwidthParameterId,
            juce::Colour accentToUse)
            : drive (driveEffect),
              title (std::move (titleToUse)),
              accent (accentToUse),
              gainLabel (
                  state.getParameter (gainParameterId),
                  state.getRawParameterValue (gainParameterId),
                  EditableParameterLabel::Unit::Decibels),
              frequencyLabel (
                  state.getParameter (frequencyParameterId),
                  state.getRawParameterValue (frequencyParameterId),
                  EditableParameterLabel::Unit::Frequency),
              bandwidthLabel (
                  state.getParameter (bandwidthParameterId),
                  state.getRawParameterValue (bandwidthParameterId),
                  EditableParameterLabel::Unit::Bandwidth)
        {
            enabledValue = state.getRawParameterValue (
                enableParameterId);
            gainValue = state.getRawParameterValue (
                gainParameterId);
            frequencyValue = state.getRawParameterValue (
                frequencyParameterId);
            bandwidthValue = state.getRawParameterValue (
                bandwidthParameterId);

            gainParameter = state.getParameter (
                gainParameterId);
            frequencyParameter = state.getParameter (
                frequencyParameterId);
            bandwidthParameter = state.getParameter (
                bandwidthParameterId);

            addAndMakeVisible (gainLabel);
            addAndMakeVisible (frequencyLabel);
            addAndMakeVisible (bandwidthLabel);

            setTooltip (
                title
                + ": horizontal drag changes frequency, vertical drag changes gain, "
                  "and Alt/Option + vertical drag changes bandwidth. "
                  "Click a value below the graph to type it.");
            setMouseCursor (juce::MouseCursor::CrosshairCursor);
        }

        void refreshValueLabels()
        {
            gainLabel.refresh();
            frequencyLabel.refresh();
            bandwidthLabel.refresh();
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            bounds.removeFromTop (22);
            auto values = bounds.removeFromBottom (24);

            const int gainWidth = juce::roundToInt (
                values.getWidth() * 0.27f);
            const int bandwidthWidth = juce::roundToInt (
                values.getWidth() * 0.25f);

            gainLabel.setBounds (values.removeFromLeft (gainWidth));
            bandwidthLabel.setBounds (
                values.removeFromRight (bandwidthWidth));
            frequencyLabel.setBounds (values);
        }

        void paint (juce::Graphics& graphics) override
        {
            auto bounds = getLocalBounds().toFloat();
            auto titleArea = bounds.removeFromTop (22.0f);
            bounds.removeFromBottom (24.0f);
            auto graph = bounds.reduced (5.0f, 3.0f);

            const bool enabled = enabledValue != nullptr
                && enabledValue->load (std::memory_order_relaxed) > 0.5f;

            graphics.setColour (enabled ? accent : Palette::textDim);
            graphics.setFont (FontBank::font (11.5f, true));
            graphics.drawText (
                title,
                titleArea.toNearestInt(),
                juce::Justification::centredLeft);

            graphics.setColour (Palette::panel.withAlpha (0.90f));
            graphics.fillRoundedRectangle (graph, 5.0f);

            drawSpectrum (graphics, graph);
            drawGrid (graphics, graph);
            drawBandCurve (graphics, graph, enabled);

            graphics.setColour (Palette::stroke.withAlpha (0.86f));
            graphics.drawRoundedRectangle (
                graph,
                5.0f,
                1.0f);
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            if (! graphBounds().contains (event.position))
                return;

            beginGestures();
            dragStart = event.position;
            startBandwidthNormalised = bandwidthParameter != nullptr
                ? bandwidthParameter->getValue()
                : 0.5f;
            updateFromEvent (event);
        }

        void mouseDrag (const juce::MouseEvent& event) override
        {
            if (gestureActive)
                updateFromEvent (event);
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            endGestures();
        }

        void mouseDoubleClick (const juce::MouseEvent& event) override
        {
            if (! graphBounds().contains (event.position))
                return;

            resetParameter (gainParameter);
            resetParameter (frequencyParameter);
            resetParameter (bandwidthParameter);
            refreshValueLabels();
            repaint();
        }

    private:
        static constexpr float lowestDisplayFrequency = 30.0f;
        static constexpr float highestDisplayFrequency = 20000.0f;

        juce::Rectangle<float> graphBounds() const
        {
            auto bounds = getLocalBounds().toFloat();
            bounds.removeFromTop (22.0f);
            bounds.removeFromBottom (24.0f);
            return bounds.reduced (5.0f, 3.0f);
        }

        static float frequencyToNormalised (float frequency) noexcept
        {
            const float safe = juce::jlimit (
                lowestDisplayFrequency,
                highestDisplayFrequency,
                frequency);
            return std::log (safe / lowestDisplayFrequency)
                / std::log (highestDisplayFrequency / lowestDisplayFrequency);
        }

        static float normalisedToFrequency (float normalised) noexcept
        {
            return lowestDisplayFrequency
                * std::pow (
                    highestDisplayFrequency / lowestDisplayFrequency,
                    juce::jlimit (0.0f, 1.0f, normalised));
        }

        static float readRaw (std::atomic<float>* value) noexcept
        {
            return value != nullptr
                ? value->load (std::memory_order_relaxed)
                : 0.0f;
        }

        void drawGrid (juce::Graphics& graphics,
                       juce::Rectangle<float> graph)
        {
            graphics.setColour (Palette::stroke.withAlpha (0.30f));

            for (float frequency : {
                     50.0f, 100.0f, 200.0f, 500.0f,
                     1000.0f, 2000.0f, 5000.0f, 10000.0f
                 })
            {
                const float x = graph.getX()
                    + frequencyToNormalised (frequency)
                        * graph.getWidth();
                graphics.drawVerticalLine (
                    juce::roundToInt (x),
                    graph.getY(),
                    graph.getBottom());
            }

            for (int index = 1; index < 4; ++index)
            {
                const float y = graph.getY()
                    + graph.getHeight() * (float) index / 4.0f;
                graphics.drawHorizontalLine (
                    juce::roundToInt (y),
                    graph.getX(),
                    graph.getRight());
            }
        }

        void drawSpectrum (juce::Graphics& graphics,
                           juce::Rectangle<float> graph)
        {
            const auto& bins = drive.getSpectrumBins();
            juce::Path outline;

            for (int bin = 0;
                 bin < DriveEffect::spectrumBinCount;
                 ++bin)
            {
                const float x = graph.getX()
                    + graph.getWidth()
                        * (float) bin
                        / (float) (DriveEffect::spectrumBinCount - 1);
                const float magnitude = juce::jlimit (
                    0.0f,
                    1.0f,
                    bins[(size_t) bin].load (
                        std::memory_order_relaxed));
                const float y = graph.getBottom()
                    - magnitude * graph.getHeight() * 0.82f;

                if (bin == 0)
                    outline.startNewSubPath (x, y);
                else
                    outline.lineTo (x, y);
            }

            juce::Path fill (outline);
            fill.lineTo (graph.getRight(), graph.getBottom());
            fill.lineTo (graph.getX(), graph.getBottom());
            fill.closeSubPath();

            graphics.setColour (accent.withAlpha (0.075f));
            graphics.fillPath (fill);
            graphics.setColour (Palette::textDim.withAlpha (0.22f));
            graphics.strokePath (
                outline,
                juce::PathStrokeType (
                    1.2f,
                    juce::PathStrokeType::curved,
                    juce::PathStrokeType::rounded));
        }

        void drawBandCurve (juce::Graphics& graphics,
                            juce::Rectangle<float> graph,
                            bool enabled)
        {
            const float gain = readRaw (gainValue);
            const float frequency = readRaw (frequencyValue);
            const float bandwidth = readRaw (bandwidthValue);
            const float gainNormalised = gainParameter != nullptr
                ? gainParameter->convertTo0to1 (gain)
                : 0.0f;
            const float q = juce::jmap (
                juce::jlimit (0.20f, 4.0f, bandwidth),
                0.20f,
                4.0f,
                5.0f,
                0.35f);
            const float centreY = graph.getCentreY();

            juce::Path curve;

            for (int index = 0; index <= 128; ++index)
            {
                const float normalisedX = (float) index / 128.0f;
                const float currentFrequency = normalisedToFrequency (
                    normalisedX);
                const float ratio = currentFrequency
                    / juce::jmax (1.0f, frequency);
                const float response = 1.0f
                    / std::sqrt (
                        1.0f
                        + std::pow (
                            q * (ratio - 1.0f / ratio),
                            2.0f));
                const float x = graph.getX()
                    + normalisedX * graph.getWidth();
                const float y = centreY
                    - response
                        * gainNormalised
                        * graph.getHeight()
                        * 0.43f;

                if (index == 0)
                    curve.startNewSubPath (x, y);
                else
                    curve.lineTo (x, y);
            }

            graphics.setColour (accent.withAlpha (
                enabled ? 0.98f : 0.34f));
            graphics.strokePath (
                curve,
                juce::PathStrokeType (
                    2.0f,
                    juce::PathStrokeType::curved,
                    juce::PathStrokeType::rounded));

            const float pointX = graph.getX()
                + frequencyToNormalised (frequency)
                    * graph.getWidth();
            const float pointY = centreY
                - gainNormalised
                    * graph.getHeight()
                    * 0.43f;

            graphics.setColour (enabled ? accent : Palette::textDim);
            graphics.fillEllipse (
                pointX - 4.5f,
                pointY - 4.5f,
                9.0f,
                9.0f);
        }

        static void resetParameter (juce::RangedAudioParameter* parameter)
        {
            if (parameter == nullptr)
                return;

            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
            parameter->endChangeGesture();
        }

        void beginGestures()
        {
            if (gestureActive)
                return;

            gestureActive = true;
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
            if (! gestureActive)
                return;

            for (auto* parameter : {
                     gainParameter,
                     frequencyParameter,
                     bandwidthParameter
                 })
            {
                if (parameter != nullptr)
                    parameter->endChangeGesture();
            }

            gestureActive = false;
        }

        void updateFromEvent (const juce::MouseEvent& event)
        {
            const auto graph = graphBounds();

            if (event.mods.isAltDown())
            {
                if (bandwidthParameter != nullptr)
                {
                    const float delta = (dragStart.y - event.position.y)
                        / juce::jmax (1.0f, graph.getHeight());
                    bandwidthParameter->setValueNotifyingHost (
                        juce::jlimit (
                            0.0f,
                            1.0f,
                            startBandwidthNormalised + delta));
                }
            }
            else
            {
                if (frequencyParameter != nullptr)
                {
                    const float normalisedX = juce::jlimit (
                        0.0f,
                        1.0f,
                        (event.position.x - graph.getX())
                            / juce::jmax (1.0f, graph.getWidth()));
                    const float rawFrequency = normalisedToFrequency (
                        normalisedX);
                    frequencyParameter->setValueNotifyingHost (
                        frequencyParameter->convertTo0to1 (
                            juce::jlimit (
                                frequencyParameter->getNormalisableRange().start,
                                frequencyParameter->getNormalisableRange().end,
                                rawFrequency)));
                }

                if (gainParameter != nullptr)
                {
                    gainParameter->setValueNotifyingHost (
                        juce::jlimit (
                            0.0f,
                            1.0f,
                            (graph.getBottom() - event.position.y)
                                / juce::jmax (1.0f, graph.getHeight())));
                }
            }

            refreshValueLabels();
            repaint();
        }

        DriveEffect& drive;
        juce::String title;
        juce::Colour accent;

        EditableParameterLabel gainLabel;
        EditableParameterLabel frequencyLabel;
        EditableParameterLabel bandwidthLabel;

        std::atomic<float>* enabledValue = nullptr;
        std::atomic<float>* gainValue = nullptr;
        std::atomic<float>* frequencyValue = nullptr;
        std::atomic<float>* bandwidthValue = nullptr;

        juce::RangedAudioParameter* gainParameter = nullptr;
        juce::RangedAudioParameter* frequencyParameter = nullptr;
        juce::RangedAudioParameter* bandwidthParameter = nullptr;

        juce::Point<float> dragStart;
        float startBandwidthNormalised = 0.5f;
        bool gestureActive = false;
    };

    class DrivePhasePanel final : public juce::Component,
                                  private juce::Timer
    {
    public:
        DrivePhasePanel (
            juce::AudioProcessorValueTreeState& state,
            DriveEffect& driveEffect)
            : tracePad (
                  state,
                  driveEffect,
                  "TRACING MODEL",
                  "drive_trace_enabled",
                  "drive_trace_gain",
                  "drive_trace_freq",
                  "drive_trace_bandwidth",
                  Palette::orange),
              pinchPad (
                  state,
                  driveEffect,
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
                "Soft gives a smoother dubplate-like rectification. Hard gives a sharper record-like geometric distortion.");
            stereoPinch.button.setTooltip (
                "Send the Pinch component to the right channel with opposite polarity for a realistic stereo groove image.");

            startTimerHz (24);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (5);
            auto controls = bounds.removeFromTop (52);

            const int gap = 8;
            const int itemWidth = (controls.getWidth() - gap * 3) / 4;

            traceEnabled.setBounds (controls.removeFromLeft (itemWidth));
            controls.removeFromLeft (gap);
            pinchEnabled.setBounds (controls.removeFromLeft (itemWidth));
            controls.removeFromLeft (gap);
            character.setBounds (controls.removeFromLeft (itemWidth));
            controls.removeFromLeft (gap);
            stereoPinch.setBounds (controls);

            bounds.removeFromTop (5);
            const int padWidth = (bounds.getWidth() - 10) / 2;
            tracePad.setBounds (bounds.removeFromLeft (padWidth));
            bounds.removeFromLeft (10);
            pinchPad.setBounds (bounds);
        }

    private:
        void timerCallback() override
        {
            tracePad.refreshValueLabels();
            pinchPad.refreshValueLabels();
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
