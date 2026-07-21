#pragma once
#include "Widgets.h"

namespace mfx
{
    class StutterPanel : public juce::Component, private juce::Timer
    {
    public:
        StutterPanel (juce::AudioProcessorValueTreeState& state, EffectChain& effectChain)
            : apvts (state), chain (effectChain)
        {
            enableToggle = std::make_unique<LabeledToggle> (apvts, "stutter_enabled", "ON");
            numStepsKnob = std::make_unique<LabeledKnob> (apvts, "stutter_numsteps", "STEPS", Palette::pink);
            numStepsKnob->slider.setSliderStyle (juce::Slider::LinearHorizontal);
            divCombo = std::make_unique<LabeledCombo> (apvts, "stutter_div", "STEP RATE");
            mixKnob = std::make_unique<LabeledKnob> (apvts, "stutter_mix", "MIX", Palette::pink);

            addAndMakeVisible (*enableToggle);
            addAndMakeVisible (*numStepsKnob);
            addAndMakeVisible (*divCombo);
            addAndMakeVisible (*mixKnob);

            clearButton.setTooltip ("Set every cell to Off");
            alternateButton.setTooltip ("Create an alternating Repeat 1/8 pattern");
            clearButton.onClick = [this] { fillPattern (StepAction::Off, false); };
            alternateButton.onClick = [this] { fillPattern (StepAction::Repeat8, true); };
            addAndMakeVisible (clearButton);
            addAndMakeVisible (alternateButton);

            grid = std::make_unique<StepActionGrid> (apvts, "stutter_step");
            grid->setCurrentStepProvider ([this] { return chain.stutter.getCurrentStepIndex(); });
            addAndMakeVisible (*grid);

            legend.setJustificationType (juce::Justification::centredLeft);
            legend.setText ("Each cell lasts STEP RATE. Repeat 1/4, 1/8, 1/16 and 1/32 use absolute musical loop lengths.",
                            juce::dontSendNotification);
            legend.setColour (juce::Label::textColourId, Palette::textDim);
            legend.setFont (juce::Font (juce::FontOptions (13.0f)));
            addAndMakeVisible (legend);

            startTimerHz (12);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (14);
            auto controls = bounds.removeFromTop (96);

            enableToggle->setBounds (controls.removeFromLeft (68).reduced (0, 13));
            controls.removeFromLeft (12);
            numStepsKnob->setBounds (controls.removeFromLeft (170));
            controls.removeFromLeft (12);
            divCombo->setBounds (controls.removeFromLeft (160).removeFromTop (60));
            controls.removeFromLeft (12);
            mixKnob->setBounds (controls.removeFromLeft (108));
            controls.removeFromLeft (16);
            clearButton.setBounds (controls.removeFromLeft (78).reduced (0, 27));
            controls.removeFromLeft (8);
            alternateButton.setBounds (controls.removeFromLeft (100).reduced (0, 27));

            bounds.removeFromTop (4);
            legend.setBounds (bounds.removeFromTop (24));
            bounds.removeFromTop (6);
            grid->setBounds (bounds);
        }

    private:
        void fillPattern (StepAction action, bool alternate)
        {
            const int count = (int) apvts.getRawParameterValue ("stutter_numsteps")->load();
            for (int step = 0; step < StutterEngine::maxSteps; ++step)
            {
                const StepAction value = step < count && (! alternate || step % 2 == 0) ? action : StepAction::Off;
                if (auto* parameter = apvts.getParameter ("stutter_step" + juce::String (step)))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) value));
            }
        }

        void timerCallback() override
        {
            grid->setNumSteps ((int) apvts.getRawParameterValue ("stutter_numsteps")->load());
        }

        juce::AudioProcessorValueTreeState& apvts;
        EffectChain& chain;
        std::unique_ptr<LabeledToggle> enableToggle;
        std::unique_ptr<LabeledKnob> numStepsKnob, mixKnob;
        std::unique_ptr<LabeledCombo> divCombo;
        std::unique_ptr<StepActionGrid> grid;
        juce::TextButton clearButton { "CLEAR" };
        juce::TextButton alternateButton { "ALT 1/8" };
        juce::Label legend;
    };
}
