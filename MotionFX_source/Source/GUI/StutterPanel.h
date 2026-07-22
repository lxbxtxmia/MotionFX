#pragma once
#include "Widgets.h"

namespace mfx
{
    class StutterPanel : public juce::Component, private juce::Timer
    {
    public:
        StutterPanel (juce::AudioProcessorValueTreeState& state,
                      EffectChain& effectChain)
            : apvts (state), chain (effectChain)
        {
            enableToggle = std::make_unique<LabeledToggle> (apvts, "stutter_enabled", "ON");
            numStepsKnob = std::make_unique<LabeledKnob> (apvts, "stutter_numsteps", "STEPS", Palette::pink);
            numStepsKnob->slider.setSliderStyle (juce::Slider::LinearHorizontal);
            divisionCombo = std::make_unique<LabeledCombo> (apvts, "stutter_div", "STEP RATE");
            mixKnob = std::make_unique<LabeledKnob> (apvts, "stutter_mix", "MIX", Palette::pink);
            grainKnob = std::make_unique<LabeledKnob> (apvts, "stutter_pitch_grain_ms", "PITCH GRAIN", Palette::pink);

            for (auto* component : { (juce::Component*) enableToggle.get(),
                                     (juce::Component*) numStepsKnob.get(),
                                     (juce::Component*) divisionCombo.get(),
                                     (juce::Component*) mixKnob.get(),
                                     (juce::Component*) grainKnob.get() })
                addAndMakeVisible (component);

            repeatGrid = std::make_unique<StepLaneGrid> (
                apvts, "stutter_repeat_step",
                juce::StringArray { "Off", "1/4", "1/8", "1/16", "1/32" },
                Palette::teal, false, true);
            reverseGrid = std::make_unique<StepLaneGrid> (
                apvts, "stutter_reverse_step", juce::StringArray { "Off", "On" },
                Palette::purple, true, false);
            tapeGrid = std::make_unique<StepLaneGrid> (
                apvts, "stutter_tape_step", juce::StringArray { "Off", "Down", "Up" },
                Palette::orange, false, true);
            pitchGrid = std::make_unique<PitchStepGrid> (
                apvts, "stutter_pitch_step", "stutter_pitch_semitones_step", Palette::pink);
            gateGrid = std::make_unique<StepLaneGrid> (
                apvts, "stutter_gate_step", juce::StringArray { "Off", "On" },
                Palette::red, true, false);

            for (auto* grid : { repeatGrid.get(), reverseGrid.get(), tapeGrid.get(), gateGrid.get() })
            {
                grid->setCurrentStepProvider ([this] { return chain.stutter.getCurrentStepIndex(); });
                addAndMakeVisible (grid);
            }
            pitchGrid->setCurrentStepProvider ([this] { return chain.stutter.getCurrentStepIndex(); });
            addAndMakeVisible (*pitchGrid);

            configureLabel (repeatLabel, "REPEAT", Palette::teal);
            configureLabel (reverseLabel, "REVERSE", Palette::purple);
            configureLabel (tapeLabel, "TAPE", Palette::orange);
            configureLabel (pitchLabel, "PITCH", Palette::pink);
            configureLabel (gateLabel, "GATE", Palette::red);

            interactionHint.setText (
                "L-drag paint  |  R-drag erase  |  Pitch: drag up/down, click = 0 st",
                juce::dontSendNotification);
            interactionHint.setJustificationType (juce::Justification::centredLeft);
            interactionHint.setColour (juce::Label::textColourId, Palette::textDim);
            interactionHint.setFont (juce::Font (juce::FontOptions (11.5f)));
            addAndMakeVisible (interactionHint);

            clearButton.setTooltip ("Clear every Stutter lane");
            clearButton.onClick = [this] { clearAllLanes(); };
            addAndMakeVisible (clearButton);
            startTimerHz (12);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (14);
            auto controls = bounds.removeFromTop (100);
            enableToggle->setBounds (controls.removeFromLeft (68).reduced (0, 13));
            controls.removeFromLeft (10);
            numStepsKnob->setBounds (controls.removeFromLeft (160));
            controls.removeFromLeft (10);
            divisionCombo->setBounds (controls.removeFromLeft (160).removeFromTop (62));
            controls.removeFromLeft (10);
            mixKnob->setBounds (controls.removeFromLeft (110));
            controls.removeFromLeft (10);
            grainKnob->setBounds (controls.removeFromLeft (126));
            controls.removeFromLeft (12);
            clearButton.setBounds (controls.removeFromLeft (78).reduced (0, 30));

            interactionHint.setBounds (bounds.removeFromTop (22));
            bounds.removeFromTop (5);
            const int gap = 6;
            const int rowHeight = juce::jmax (42, (bounds.getHeight() - gap * 4) / 5);
            layoutLane (repeatLabel, *repeatGrid, bounds.removeFromTop (rowHeight));
            bounds.removeFromTop (gap);
            layoutLane (reverseLabel, *reverseGrid, bounds.removeFromTop (rowHeight));
            bounds.removeFromTop (gap);
            layoutLane (tapeLabel, *tapeGrid, bounds.removeFromTop (rowHeight));
            bounds.removeFromTop (gap);
            layoutLane (pitchLabel, *pitchGrid, bounds.removeFromTop (rowHeight));
            bounds.removeFromTop (gap);
            layoutLane (gateLabel, *gateGrid, bounds);
        }

    private:
        void configureLabel (juce::Label& label, const juce::String& text, juce::Colour colour)
        {
            label.setText (text, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centredLeft);
            label.setColour (juce::Label::textColourId, colour);
            label.setFont (juce::Font (juce::FontOptions (12.0f)).withStyle (juce::Font::bold));
            addAndMakeVisible (label);
        }

        template <typename GridType>
        static void layoutLane (juce::Label& label, GridType& grid, juce::Rectangle<int> row)
        {
            label.setBounds (row.removeFromLeft (82));
            row.removeFromLeft (6);
            grid.setBounds (row);
        }

        void clearAllLanes()
        {
            for (int step = 0; step < StutterEngine::maxSteps; ++step)
            {
                const juce::String index (step);
                setParameter ("stutter_repeat_step" + index, 0.0f);
                setParameter ("stutter_reverse_step" + index, 0.0f);
                setParameter ("stutter_tape_step" + index, 0.0f);
                setParameter ("stutter_pitch_step" + index, 0.0f);
                setParameter ("stutter_pitch_semitones_step" + index, 0.0f);
                setParameter ("stutter_gate_step" + index, 0.0f);
            }
        }

        void setParameter (const juce::String& parameterId, float value)
        {
            if (auto* parameter = apvts.getParameter (parameterId))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }

        void timerCallback() override
        {
            const int steps = (int) apvts.getRawParameterValue ("stutter_numsteps")->load();
            for (auto* grid : { repeatGrid.get(), reverseGrid.get(), tapeGrid.get(), gateGrid.get() })
                grid->setNumSteps (steps);
            pitchGrid->setNumSteps (steps);
        }

        juce::AudioProcessorValueTreeState& apvts;
        EffectChain& chain;
        std::unique_ptr<LabeledToggle> enableToggle;
        std::unique_ptr<LabeledKnob> numStepsKnob, mixKnob, grainKnob;
        std::unique_ptr<LabeledCombo> divisionCombo;
        std::unique_ptr<StepLaneGrid> repeatGrid, reverseGrid, tapeGrid, gateGrid;
        std::unique_ptr<PitchStepGrid> pitchGrid;
        juce::Label repeatLabel, reverseLabel, tapeLabel, pitchLabel, gateLabel, interactionHint;
        juce::TextButton clearButton { "CLEAR" };
    };
}
