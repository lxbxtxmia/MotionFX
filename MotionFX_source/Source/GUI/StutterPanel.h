#pragma once
#include "Widgets.h"

namespace mfx
{
    class StutterPanel : public juce::Component, private juce::Timer
    {
    public:
        StutterPanel (juce::AudioProcessorValueTreeState& s, EffectChain& c) : apvts (s), chain (c)
        {
            enableToggle = std::make_unique<LabeledToggle> (apvts, "stutter_enabled", "ON");
            addAndMakeVisible (*enableToggle);

            numStepsKnob = std::make_unique<LabeledKnob> (apvts, "stutter_numsteps", "STEPS", Palette::pink);
            numStepsKnob->slider.setSliderStyle (juce::Slider::LinearHorizontal);
            addAndMakeVisible (*numStepsKnob);

            divCombo = std::make_unique<LabeledCombo> (apvts, "stutter_div", "DIVISION");
            addAndMakeVisible (*divCombo);

            mixKnob = std::make_unique<LabeledKnob> (apvts, "stutter_mix", "MIX", Palette::pink);
            addAndMakeVisible (*mixKnob);

            grid = std::make_unique<StepActionGrid> (apvts, "stutter_step");
            grid->setCurrentStepProvider ([this] { return chain.stutter.getCurrentStepIndex(); });
            addAndMakeVisible (*grid);

            legend.setJustificationType (juce::Justification::centredLeft);
            legend.setText ("Click a cell to cycle its action, drag to paint. Pattern length & rate set the grid above.",
                             juce::dontSendNotification);
            legend.setColour (juce::Label::textColourId, Palette::textDim);
            legend.setFont (juce::Font (juce::FontOptions (13.0f)));
            addAndMakeVisible (legend);

            startTimerHz (10);
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (14);
            auto top = b.removeFromTop (64);
            enableToggle->setBounds (top.removeFromLeft (54));
            top.removeFromLeft (10);
            numStepsKnob->setBounds (top.removeFromLeft (140));
            top.removeFromLeft (10);
            divCombo->setBounds (top.removeFromLeft (110));
            top.removeFromLeft (10);
            mixKnob->setBounds (top.removeFromLeft (64));

            b.removeFromTop (10);
            legend.setBounds (b.removeFromTop (20));
            b.removeFromTop (6);
            grid->setBounds (b);
        }

    private:
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
        juce::Label legend;
    };
}
