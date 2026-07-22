#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"
#include "../DSP/EffectChain.h"
#include "../Parameters.h"
#include <deque>

namespace mfx
{
    class EditableSlider : public juce::Slider
    {
    public:
        void mouseDoubleClick (const juce::MouseEvent& event) override
        {
            const auto relativeEvent = event.getEventRelativeTo (this);
            const int textBoxTop = getHeight() - getTextBoxHeight() - 2;

            if (getTextBoxPosition() != juce::Slider::NoTextBox
                && relativeEvent.position.y >= (float) textBoxTop)
            {
                showTextBox();
                return;
            }

            setValue (0.0, juce::sendNotificationSync);
        }
    };

    //==============================================================================
    class LabeledKnob : public juce::Component
    {
    public:
        LabeledKnob (juce::AudioProcessorValueTreeState& apvts,
                     const juce::String& paramId,
                     const juce::String& labelText,
                     juce::Colour accent)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 92, 22);
            slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
            slider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
            slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            addAndMakeVisible (slider);

            label.setText (labelText, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setColour (juce::Label::textColourId, Palette::textDim);
            label.addMouseListener (this, false);
            addAndMakeVisible (label);

            auto* parameter = apvts.getParameter (paramId);
            decimalPlaces = dynamic_cast<juce::AudioParameterInt*> (parameter) != nullptr ? 0 : 2;
            defaultUnitSuffix = inferUnitSuffix (paramId, parameter);

            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, paramId, slider);

            // SliderAttachment installs parameter formatting. MotionFX formatting is
            // applied afterwards so values remain compact and include their unit.
            restoreDefaultFormatter();
            setTooltipText (labelText
                            + " - double-click knob for zero; double-click value or label to type");
        }

        void setCompactLayout (bool shouldBeCompact)
        {
            compactLayout = shouldBeCompact;
            resized();
        }

        void resized() override
        {
            auto bounds = getLocalBounds();

            if (compactLayout)
            {
                label.setBounds (bounds.removeFromBottom (14));
                slider.setTextBoxStyle (
                    juce::Slider::TextBoxBelow,
                    false,
                    72,
                    18);
                slider.setBounds (bounds.reduced (2, 0));
                return;
            }

            label.setBounds (bounds.removeFromBottom (18));
            slider.setTextBoxStyle (
                juce::Slider::TextBoxBelow,
                false,
                92,
                22);
            slider.setBounds (bounds);
        }

        void mouseDoubleClick (const juce::MouseEvent& event) override
        {
            if (event.eventComponent == &label)
                slider.showTextBox();
        }

        void setLabelText (const juce::String& text)
        {
            if (label.getText() != text)
                label.setText (text, juce::dontSendNotification);
        }

        void setTooltipText (const juce::String& text)
        {
            slider.setTooltip (text);
            label.setTooltip (text);
        }

        void setFixedDecimals (int decimals)
        {
            decimalPlaces = decimals;
            restoreDefaultFormatter();
        }

        void setUnitSuffix (const juce::String& suffix)
        {
            installNumericFormatter (decimalPlaces, suffix);
        }

        void restoreDefaultFormatter()
        {
            installNumericFormatter (decimalPlaces, defaultUnitSuffix);
        }

        void setTextFunctions (std::function<juce::String (double)> formatter,
                               std::function<double (const juce::String&)> parser)
        {
            slider.textFromValueFunction = std::move (formatter);
            slider.valueFromTextFunction = std::move (parser);
            slider.updateText();
        }

        EditableSlider slider;

    private:
        static juce::String inferUnitSuffix (const juce::String& paramId,
                                             juce::RangedAudioParameter* parameter)
        {
            if (paramId.endsWith ("_pitch_grain_ms"))
                return " ms";

            if (dynamic_cast<juce::AudioParameterInt*> (parameter) != nullptr)
                return {};

            if (paramId == "master_input" || paramId == "master_output"
                || paramId == "drive_outtrim")
                return " dB";

            if (paramId.endsWith ("_env_attack")
                || paramId.endsWith ("_env_release")
                || paramId.endsWith ("_seq_smooth"))
                return " ms";

            if (paramId.endsWith ("_lfo_rate")
                || paramId.endsWith ("_motion_rate")
                || paramId.endsWith ("_seq_rate"))
                return " Hz";

            return " %";
        }

        void installNumericFormatter (int decimals, const juce::String& suffix)
        {
            slider.setNumDecimalPlacesToDisplay (decimals);
            slider.textFromValueFunction = [decimals, suffix] (double value)
            {
                return juce::String (value, decimals) + suffix;
            };
            slider.valueFromTextFunction = [] (const juce::String& text)
            {
                return text.getDoubleValue();
            };
            slider.updateText();
        }

        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::String defaultUnitSuffix;
        int decimalPlaces = 2;
        bool compactLayout = false;
    };

    //==============================================================================
    class LabeledCombo : public juce::Component
    {
    public:
        LabeledCombo (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& labelText)
        {
            combo.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (combo);
            label.setText (labelText, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setColour (juce::Label::textColourId, Palette::textDim);
            addAndMakeVisible (label);

            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramId)))
            {
                combo.addItemList (p->choices, 1);
            }
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramId, combo);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            if (label.getText().isNotEmpty())
                label.setBounds (b.removeFromTop (16));
            combo.setBounds (b.reduced (0, 1));
        }

        juce::ComboBox combo;
    private:
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    //==============================================================================
    class LabeledToggle : public juce::Component
    {
    public:
        LabeledToggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& labelText)
        {
            button.setButtonText (labelText);
            addAndMakeVisible (button);
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramId, button);
        }
        void resized() override { button.setBounds (getLocalBounds()); }
        juce::ToggleButton button;
    private:
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    //==============================================================================
    // Live input/output signal envelopes are drawn behind the modulation trace.
    class ModVisualizer : public juce::Component, private juce::Timer
    {
    public:
        ModVisualizer (std::atomic<float>& modulationSource,
                       std::atomic<float>& inputSource,
                       std::atomic<float>& outputSource,
                       std::atomic<juce::uint64>& epochSource,
                       juce::Colour accentColour)
            : modulation (modulationSource), input (inputSource), output (outputSource),
              signalEpoch (epochSource), accent (accentColour)
        {
            modulationHistory.assign (180, 0.0f);
            inputHistory.assign (180, 0.0f);
            outputHistory.assign (180, 0.0f);
            lastEpoch = signalEpoch.load (std::memory_order_relaxed);
            startTimerHz (30);
        }

        void setActive (bool shouldShowModulation)
        {
            modulationActive = shouldShowModulation;
        }

        void paint (juce::Graphics& graphics) override
        {
            const auto bounds = getLocalBounds().toFloat();
            graphics.setColour (Palette::bg1);
            graphics.fillRoundedRectangle (bounds, 6.0f);
            graphics.setColour (Palette::stroke);
            graphics.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

            const auto inner = bounds.reduced (5.0f);
            graphics.setColour (Palette::stroke.withAlpha (0.45f));
            graphics.drawHorizontalLine ((int) inner.getCentreY(), inner.getX(), inner.getRight());

            drawEnvelope (graphics, inputHistory, Palette::textDim.withAlpha (0.32f), inner, false);
            drawEnvelope (graphics, outputHistory, accent.withAlpha (0.55f), inner, true);
            if (modulationActive)
                drawModulation (graphics, inner);

            auto labelBounds = inner.toNearestInt().removeFromTop (14);
            graphics.setFont (FontBank::font (10.0f));
            graphics.setColour (Palette::textDim.withAlpha (0.75f));
            graphics.drawText ("IN", labelBounds, juce::Justification::topLeft);
            graphics.setColour (accent.withAlpha (0.85f));
            graphics.drawText (modulationActive ? "OUT + MOD" : "OUT",
                               labelBounds, juce::Justification::topRight);
        }

    private:
        static void drawEnvelope (juce::Graphics& graphics,
                                  const std::deque<float>& history,
                                  juce::Colour colour,
                                  juce::Rectangle<float> area,
                                  bool fill)
        {
            if (history.size() < 2)
                return;

            juce::Path upper, lower;
            const float centreY = area.getCentreY();
            const float halfHeight = area.getHeight() * 0.46f;

            for (size_t index = 0; index < history.size(); ++index)
            {
                const float x = area.getX()
                    + area.getWidth() * ((float) index / (float) (history.size() - 1));
                const float amplitude = juce::jlimit (0.0f, 1.0f, history[index]) * halfHeight;
                if (index == 0)
                {
                    upper.startNewSubPath (x, centreY - amplitude);
                    lower.startNewSubPath (x, centreY + amplitude);
                }
                else
                {
                    upper.lineTo (x, centreY - amplitude);
                    lower.lineTo (x, centreY + amplitude);
                }
            }

            graphics.setColour (colour);
            graphics.strokePath (upper, juce::PathStrokeType (1.4f));
            graphics.strokePath (lower, juce::PathStrokeType (1.4f));

            if (fill)
            {
                juce::Path shape (upper);
                for (size_t reverseIndex = history.size(); reverseIndex-- > 0;)
                {
                    const float x = area.getX()
                        + area.getWidth() * ((float) reverseIndex / (float) (history.size() - 1));
                    const float amplitude = juce::jlimit (0.0f, 1.0f, history[reverseIndex]) * halfHeight;
                    shape.lineTo (x, centreY + amplitude);
                }
                shape.closeSubPath();
                graphics.setColour (colour.withAlpha (colour.getFloatAlpha() * 0.22f));
                graphics.fillPath (shape);
            }
        }

        void drawModulation (juce::Graphics& graphics, juce::Rectangle<float> area)
        {
            juce::Path path;
            for (size_t index = 0; index < modulationHistory.size(); ++index)
            {
                const float x = area.getX()
                    + area.getWidth() * ((float) index / (float) (modulationHistory.size() - 1));
                const float y = area.getBottom() - modulationHistory[index] * area.getHeight();
                if (index == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
            }
            graphics.setColour (accent.brighter (0.25f));
            graphics.strokePath (path, juce::PathStrokeType (
                2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const float last = modulationHistory.back();
            graphics.fillEllipse (area.getRight() - 4.0f,
                                  area.getBottom() - last * area.getHeight() - 4.0f,
                                  8.0f, 8.0f);
        }

        static float visualLevel (float level) noexcept
        {
            return std::sqrt (juce::jlimit (0.0f, 1.0f, level * 1.6f));
        }

        static float decayLevel (float previous) noexcept
        {
            const float decayed = previous * 0.72f;
            return decayed < 0.001f ? 0.0f : decayed;
        }

        void timerCallback() override
        {
            const auto epoch = signalEpoch.load (std::memory_order_relaxed);

            if (epoch != lastEpoch)
            {
                lastEpoch = epoch;
                staleTicks = 0;
            }
            else
            {
                ++staleTicks;
            }

            const float modulationValue = juce::jlimit (
                0.0f,
                1.0f,
                modulation.load (std::memory_order_relaxed));

            const bool signalFresh = staleTicks <= 2;
            const float inputValue = signalFresh
                ? visualLevel (input.load (std::memory_order_relaxed))
                : decayLevel (inputHistory.back());
            const float outputValue = signalFresh
                ? visualLevel (output.load (std::memory_order_relaxed))
                : decayLevel (outputHistory.back());

            if (UiPreferences::instance().isReducedMotion())
            {
                std::fill (
                    modulationHistory.begin(),
                    modulationHistory.end(),
                    modulationValue);
                std::fill (
                    inputHistory.begin(),
                    inputHistory.end(),
                    inputValue);
                std::fill (
                    outputHistory.begin(),
                    outputHistory.end(),
                    outputValue);
            }
            else
            {
                modulationHistory.pop_front();
                inputHistory.pop_front();
                outputHistory.pop_front();
                modulationHistory.push_back (modulationValue);
                inputHistory.push_back (inputValue);
                outputHistory.push_back (outputValue);
            }

            repaint();
        }

        std::atomic<float>& modulation;
        std::atomic<float>& input;
        std::atomic<float>& output;
        std::atomic<juce::uint64>& signalEpoch;
        juce::Colour accent;
        std::deque<float> modulationHistory, inputHistory, outputHistory;
        juce::uint64 lastEpoch = 0;
        int staleTicks = 0;
        bool modulationActive = false;
    };

    //==============================================================================
    // Draggable bar-graph editor for the step sequencer (bound directly to
    // <prefix>_seq_step0..31 float params -- no per-step attachments needed).
    class StepBarGrid : public juce::Component
    {
    public:
        StepBarGrid (juce::AudioProcessorValueTreeState& s, juce::String pfx, juce::Colour accentColour)
            : apvts (s), prefix (std::move (pfx)), accent (accentColour) {}

        void setNumSteps (int n) { numSteps = juce::jlimit (1, StepSequencer::maxSteps, n); repaint(); }
        void setCurrentStepProvider (std::function<int()> fn) { currentStepFn = std::move (fn); }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (Palette::bg1);
            g.fillRoundedRectangle (b, 6.0f);

            float cellW = b.getWidth() / (float) numSteps;
            int activeStep = currentStepFn ? currentStepFn() : -1;

            for (int i = 0; i < numSteps; ++i)
            {
                auto* v = apvts.getRawParameterValue (prefix + juce::String (i));
                float val01 = v != nullptr ? juce::jlimit (0.0f, 1.0f, v->load() / 100.0f) : 1.0f;

                auto cell = juce::Rectangle<float> (b.getX() + i * cellW, b.getY(), cellW, b.getHeight()).reduced (1.5f);
                g.setColour (i == activeStep ? Palette::panelHi.brighter (0.15f) : Palette::panel);
                g.fillRoundedRectangle (cell, 2.0f);

                auto bar = cell.withY (cell.getBottom() - cell.getHeight() * val01).withHeight (cell.getHeight() * val01);
                g.setColour (i == activeStep ? accent.brighter (0.3f) : accent.withAlpha (0.8f));
                g.fillRoundedRectangle (bar, 2.0f);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override { setFromMouse (e); }
        void mouseDrag (const juce::MouseEvent& e) override { setFromMouse (e); }

    private:
        void setFromMouse (const juce::MouseEvent& e)
        {
            float cellW = (float) getWidth() / (float) numSteps;
            int idx = juce::jlimit (0, numSteps - 1, (int) (e.position.x / cellW));
            float val01 = juce::jlimit (0.0f, 1.0f, 1.0f - (float) e.position.y / (float) getHeight());

            if (auto* p = apvts.getParameter (prefix + juce::String (idx)))
                p->setValueNotifyingHost (p->convertTo0to1 (val01 * 100.0f));
            repaint();
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::String prefix;
        juce::Colour accent;
        int numSteps = 8;
        std::function<int()> currentStepFn;
    };

    //==============================================================================
    // Left-click/drag paints an action; right-click/drag erases it.
    class StepLaneGrid : public juce::Component,
                         public juce::SettableTooltipClient
    {
    public:
        StepLaneGrid (juce::AudioProcessorValueTreeState& state,
                      juce::String parameterPrefix,
                      juce::StringArray valueNames,
                      juce::Colour laneColour,
                      bool toggle,
                      bool showText)
            : apvts (state), prefix (std::move (parameterPrefix)),
              choices (std::move (valueNames)), accent (laneColour),
              toggleMode (toggle), showChoiceText (showText)
        {
            setTooltip ("Left-drag to paint; right-drag to erase");
        }

        void setNumSteps (int steps)
        {
            const int newCount = juce::jlimit (1, StutterEngine::maxSteps, steps);
            if (newCount != numSteps) { numSteps = newCount; repaint(); }
        }
        void setCurrentStepProvider (std::function<int()> provider) { currentStepProvider = std::move (provider); }

        void paint (juce::Graphics& graphics) override
        {
            const auto bounds = getLocalBounds().toFloat();
            const float cellWidth = bounds.getWidth() / (float) numSteps;
            const int activeStep = currentStepProvider ? currentStepProvider() : -1;
            for (int step = 0; step < numSteps; ++step)
            {
                const int value = readValue (step);
                const bool enabled = value != 0;
                auto cell = juce::Rectangle<float> (bounds.getX() + step * cellWidth,
                    bounds.getY(), cellWidth, bounds.getHeight()).reduced (1.5f);
                graphics.setColour (step == activeStep ? Palette::panelHi : Palette::bg1);
                graphics.fillRoundedRectangle (cell, 3.0f);
                if (enabled)
                {
                    const float strength = choices.size() > 2
                        ? juce::jmap ((float) value, 1.0f, (float) juce::jmax (1, choices.size() - 1), 0.55f, 0.95f)
                        : 0.82f;
                    graphics.setColour (accent.withAlpha (strength));
                    graphics.fillRoundedRectangle (cell.reduced (2.0f), 2.0f);
                }
                graphics.setColour (step == activeStep ? accent.withAlpha (0.9f) : Palette::stroke);
                graphics.drawRoundedRectangle (cell, 3.0f, step == activeStep ? 1.5f : 1.0f);
                if (enabled && showChoiceText && cellWidth > 28.0f && value < choices.size())
                {
                    graphics.setColour (Palette::bg0);
                    graphics.setFont (FontBank::font (juce::jmin (10.0f, cell.getHeight() * 0.24f)));
                    graphics.drawFittedText (choices[value], cell.toNearestInt().reduced (2), juce::Justification::centred, 1);
                }
            }
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            lastPaintedStep = -1;
            const int step = stepFromX (event.position.x);
            if (event.mods.isRightButtonDown())
                paintValue = 0;
            else
            {
                const int current = readValue (step);
                if (toggleMode) paintValue = 1;
                else if (current == 0) paintValue = lastNonZeroValue;
                else
                {
                    paintValue = current + 1;
                    if (paintValue >= choices.size()) paintValue = 1;
                    lastNonZeroValue = paintValue;
                }
            }
            paintStep (step);
        }
        void mouseDrag (const juce::MouseEvent& event) override
        {
            if (event.mods.isRightButtonDown()) paintValue = 0;
            paintStep (stepFromX (event.position.x));
        }
        void mouseUp (const juce::MouseEvent&) override { lastPaintedStep = -1; }

    private:
        int stepFromX (float x) const noexcept
        {
            return juce::jlimit (0, numSteps - 1,
                (int) (x / ((float) getWidth() / (float) numSteps)));
        }
        int readValue (int step) const
        {
            if (auto* value = apvts.getRawParameterValue (prefix + juce::String (step)))
                return juce::jmax (0, (int) std::round (value->load()));
            return 0;
        }
        void paintStep (int step)
        {
            if (step == lastPaintedStep) return;
            lastPaintedStep = step;
            if (auto* parameter = apvts.getParameter (prefix + juce::String (step)))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) paintValue));
            repaint();
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::String prefix;
        juce::StringArray choices;
        juce::Colour accent;
        bool toggleMode = false, showChoiceText = false;
        int numSteps = 16, lastPaintedStep = -1, paintValue = 1, lastNonZeroValue = 1;
        std::function<int()> currentStepProvider;
    };

    //==============================================================================
    class PitchStepGrid : public juce::Component,
                          public juce::SettableTooltipClient
    {
    public:
        PitchStepGrid (juce::AudioProcessorValueTreeState& state,
                       juce::String activeParameterPrefix,
                       juce::String semitoneParameterPrefix,
                       juce::Colour laneColour)
            : apvts (state), activePrefix (std::move (activeParameterPrefix)),
              semitonePrefix (std::move (semitoneParameterPrefix)), accent (laneColour)
        {
            setTooltip ("Left click = 0 st; drag up/down by semitone; right-drag erase");
        }

        void setNumSteps (int steps)
        {
            const int newCount = juce::jlimit (1, StutterEngine::maxSteps, steps);
            if (newCount != numSteps) { numSteps = newCount; repaint(); }
        }
        void setCurrentStepProvider (std::function<int()> provider) { currentStepProvider = std::move (provider); }

        static int semitonesFromDrag (int startSemitones, float verticalDelta, float pixelsPerSemitone) noexcept
        {
            return juce::jlimit (-24, 24, startSemitones
                + juce::roundToInt (verticalDelta / juce::jmax (1.0f, pixelsPerSemitone)));
        }

        void paint (juce::Graphics& graphics) override
        {
            const auto bounds = getLocalBounds().toFloat();
            const float cellWidth = bounds.getWidth() / (float) numSteps;
            const int currentStep = currentStepProvider ? currentStepProvider() : -1;
            for (int step = 0; step < numSteps; ++step)
            {
                const bool active = readActive (step);
                const int semitones = readSemitones (step);
                auto cell = juce::Rectangle<float> (bounds.getX() + step * cellWidth,
                    bounds.getY(), cellWidth, bounds.getHeight()).reduced (1.5f);
                graphics.setColour (step == currentStep ? Palette::panelHi : Palette::bg1);
                graphics.fillRoundedRectangle (cell, 3.0f);
                const float centreY = cell.getCentreY();
                graphics.setColour (Palette::stroke.withAlpha (0.7f));
                graphics.drawHorizontalLine ((int) centreY, cell.getX() + 2.0f, cell.getRight() - 2.0f);
                if (active)
                {
                    const float valueY = juce::jmap ((float) semitones, -24.0f, 24.0f,
                                                    cell.getBottom() - 3.0f, cell.getY() + 3.0f);
                    auto bar = juce::Rectangle<float> (cell.getX() + 3.0f,
                        juce::jmin (centreY, valueY), cell.getWidth() - 6.0f,
                        juce::jmax (3.0f, std::abs (valueY - centreY)));
                    graphics.setColour (accent.withAlpha (0.88f));
                    graphics.fillRoundedRectangle (bar, 2.0f);
                    if (cellWidth > 24.0f)
                    {
                        const juce::String valueText = juce::String (semitones > 0 ? "+" : "") + juce::String (semitones);
                        graphics.setColour (Palette::text);
                        graphics.setFont (FontBank::font (juce::jmin (10.0f, cell.getHeight() * 0.23f), true));
                        graphics.drawFittedText (valueText, cell.toNearestInt().reduced (1), juce::Justification::centred, 1);
                    }
                }
                graphics.setColour (step == currentStep ? accent.withAlpha (0.95f) : Palette::stroke);
                graphics.drawRoundedRectangle (cell, 3.0f, step == currentStep ? 1.5f : 1.0f);
            }
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            erasing = event.mods.isRightButtonDown();
            pressStep = stepFromX (event.position.x);
            pressY = event.position.y;
            dragged = false;
            lastEditedStep = -1;
            if (erasing) { eraseStep (pressStep); return; }
            dragStartSemitones = readActive (pressStep) ? readSemitones (pressStep) : 0;
            setStep (pressStep, true, dragStartSemitones);
            lastEditedStep = pressStep;
        }
        void mouseDrag (const juce::MouseEvent& event) override
        {
            const int step = stepFromX (event.position.x);
            if (event.mods.isRightButtonDown() || erasing)
            {
                erasing = true;
                if (step != lastEditedStep) eraseStep (step);
                return;
            }
            if (event.getDistanceFromDragStart() > 2) dragged = true;
            const int semitones = semitonesFromDrag (dragStartSemitones,
                pressY - event.position.y, juce::jmax (2.0f, (float) getHeight() / 48.0f));
            setStep (step, true, semitones);
            lastEditedStep = step;
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (! erasing && ! dragged && pressStep >= 0) setStep (pressStep, true, 0);
            erasing = false; dragged = false; pressStep = -1; lastEditedStep = -1;
        }

    private:
        int stepFromX (float x) const noexcept
        {
            return juce::jlimit (0, numSteps - 1,
                (int) (x / ((float) getWidth() / (float) numSteps)));
        }
        bool readActive (int step) const
        {
            if (auto* value = apvts.getRawParameterValue (activePrefix + juce::String (step)))
                return value->load() > 0.5f;
            return false;
        }
        int readSemitones (int step) const
        {
            if (auto* value = apvts.getRawParameterValue (semitonePrefix + juce::String (step)))
                return juce::jlimit (-24, 24, (int) std::round (value->load()));
            return 0;
        }
        void eraseStep (int step)
        {
            setParameter (activePrefix + juce::String (step), 0.0f);
            setParameter (semitonePrefix + juce::String (step), 0.0f);
            lastEditedStep = step; repaint();
        }
        void setStep (int step, bool active, int semitones)
        {
            setParameter (activePrefix + juce::String (step), active ? 1.0f : 0.0f);
            setParameter (semitonePrefix + juce::String (step), (float) juce::jlimit (-24, 24, semitones));
            repaint();
        }
        void setParameter (const juce::String& parameterId, float value)
        {
            if (auto* parameter = apvts.getParameter (parameterId))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::String activePrefix, semitonePrefix;
        juce::Colour accent;
        int numSteps = 16, pressStep = -1, lastEditedStep = -1, dragStartSemitones = 0;
        float pressY = 0.0f;
        bool erasing = false, dragged = false;
        std::function<int()> currentStepProvider;
    };

}
