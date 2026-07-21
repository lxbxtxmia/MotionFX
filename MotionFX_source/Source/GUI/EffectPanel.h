#pragma once
#include "Widgets.h"

namespace mfx
{
    struct EffectPanelSpec
    {
        juce::String prefix;
        juce::String displayName;
        juce::Colour accent;
        juce::String modeParamId;
        std::vector<std::pair<juce::String, juce::String>> secondaryKnobs;
    };

    class EffectPanel : public juce::Component, private juce::Timer
    {
    public:
        EffectPanel (juce::AudioProcessorValueTreeState& s, EffectChain& c, EffectPanelSpec spec, EffectId effectId)
            : apvts (s), chain (c), theSpec (std::move (spec)), id (effectId)
        {
            enableToggle = std::make_unique<LabeledToggle> (apvts, theSpec.prefix + "_enabled", "ON");
            modeCombo = std::make_unique<LabeledCombo> (apvts, theSpec.modeParamId, "MODE");
            primaryKnob = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_base", "AMOUNT", theSpec.accent);
            addAndMakeVisible (*enableToggle);
            addAndMakeVisible (*modeCombo);
            addAndMakeVisible (*primaryKnob);

            for (auto& sk : theSpec.secondaryKnobs)
            {
                auto knob = std::make_unique<LabeledKnob> (apvts, sk.first, sk.second, theSpec.accent);
                addAndMakeVisible (*knob);
                secondaryKnobs.push_back (std::move (knob));
            }

            visualizer = std::make_unique<ModVisualizer> (chain.uiModValue[(size_t) id], theSpec.accent);
            addAndMakeVisible (*visualizer);

            modSourceCombo = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_modsource", "MOD SOURCE");
            modDepthKnob = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_moddepth", "DEPTH", theSpec.accent);
            addAndMakeVisible (*modSourceCombo);
            addAndMakeVisible (*modDepthKnob);

            lfoShape = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_lfo_shape", "SHAPE");
            lfoSynced = std::make_unique<LabeledToggle> (apvts, theSpec.prefix + "_lfo_synced", "TEMPO SYNC");
            lfoRate = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_lfo_rate", "RATE", theSpec.accent);
            lfoRateUnit = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_lfo_rateunit", "UNIT");
            lfoDiv = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_lfo_div", "DIVISION");
            addComponents ({ lfoShape.get(), lfoSynced.get(), lfoRate.get(), lfoRateUnit.get(), lfoDiv.get() });

            envAttack = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_env_attack", "ATTACK ms", theSpec.accent);
            envRelease = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_env_release", "RELEASE ms", theSpec.accent);
            addComponents ({ envAttack.get(), envRelease.get() });

            motionShape = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_motion_shape", "SHAPE");
            motionMode = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_motion_mode", "MODE");
            motionSynced = std::make_unique<LabeledToggle> (apvts, theSpec.prefix + "_motion_synced", "TEMPO SYNC");
            motionRate = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_motion_rate", "RATE", theSpec.accent);
            motionRateUnit = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_motion_rateunit", "UNIT");
            motionDiv = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_motion_div", "DIVISION");
            motionSmooth = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_motion_smooth", "SMOOTH", theSpec.accent);
            addComponents ({ motionShape.get(), motionMode.get(), motionSynced.get(), motionRate.get(),
                             motionRateUnit.get(), motionDiv.get(), motionSmooth.get() });

            seqSteps = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_seq_numsteps", "STEPS", theSpec.accent);
            seqSteps->slider.setSliderStyle (juce::Slider::LinearHorizontal);
            seqSynced = std::make_unique<LabeledToggle> (apvts, theSpec.prefix + "_seq_synced", "TEMPO SYNC");
            seqRate = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_seq_rate", "RATE", theSpec.accent);
            seqRateUnit = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_seq_rateunit", "UNIT");
            seqDiv = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_seq_div", "STEP DIVISION");
            seqSmooth = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_seq_smooth", "SMOOTH ms", theSpec.accent);
            addComponents ({ seqSteps.get(), seqSynced.get(), seqRate.get(), seqRateUnit.get(), seqDiv.get(), seqSmooth.get() });

            seqGrid = std::make_unique<StepBarGrid> (apvts, theSpec.prefix + "_seq_step", theSpec.accent);
            seqGrid->setCurrentStepProvider ([this] { return chain.slots[(size_t) id].mod.seq.getCurrentStepIndex(); });
            addAndMakeVisible (*seqGrid);

            startTimerHz (15);
            refreshModContextVisibility();
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (12);
            const int leftWidth = juce::jlimit (310, 350, (int) (bounds.getWidth() * 0.32f));
            auto modulationArea = bounds.removeFromLeft (leftWidth);
            bounds.removeFromLeft (14);
            layoutModulationPanel (modulationArea);
            layoutMainPanel (bounds);
        }

    private:
        void addComponents (std::initializer_list<juce::Component*> components)
        {
            for (auto* component : components)
                addAndMakeVisible (component);
        }

        static void setPair (juce::Component& leftComponent, juce::Component& rightComponent,
                             juce::Rectangle<int> row, int gap = 8)
        {
            const int leftWidth = (row.getWidth() - gap) / 2;
            leftComponent.setBounds (row.removeFromLeft (leftWidth));
            row.removeFromLeft (gap);
            rightComponent.setBounds (row);
        }

        void layoutModulationPanel (juce::Rectangle<int> area)
        {
            auto sourceRow = area.removeFromTop (118);
            auto depthArea = sourceRow.removeFromRight (112);
            modDepthKnob->setBounds (depthArea);
            sourceRow.removeFromRight (10);
            modSourceCombo->setBounds (sourceRow.removeFromTop (58));
            area.removeFromTop (8);

            layoutLfo (area);
            layoutEnvelopeFollower (area);
            layoutMotion (area);
            layoutSequencer (area);
        }

        void layoutLfo (juce::Rectangle<int> area)
        {
            auto top = area.removeFromTop (58);
            setPair (*lfoShape, *lfoSynced, top);
            area.removeFromTop (12);

            auto timing = area.removeFromTop (118);
            lfoRate->setBounds (timing.removeFromLeft (118));
            timing.removeFromLeft (10);
            const auto selector = timing.removeFromTop (58);
            lfoRateUnit->setBounds (selector);
            lfoDiv->setBounds (selector);
        }

        void layoutEnvelopeFollower (juce::Rectangle<int> area)
        {
            auto row = area.removeFromTop (132);
            const int knobWidth = juce::jmin (132, (row.getWidth() - 12) / 2);
            const int total = knobWidth * 2 + 12;
            row = row.withTrimmedLeft (juce::jmax (0, (row.getWidth() - total) / 2));
            envAttack->setBounds (row.removeFromLeft (knobWidth));
            row.removeFromLeft (12);
            envRelease->setBounds (row.removeFromLeft (knobWidth));
        }

        void layoutMotion (juce::Rectangle<int> area)
        {
            auto top = area.removeFromTop (58);
            setPair (*motionShape, *motionMode, top);
            area.removeFromTop (10);

            auto timingTop = area.removeFromTop (58);
            motionSynced->setBounds (timingTop.removeFromLeft (128));
            timingTop.removeFromLeft (10);
            motionRateUnit->setBounds (timingTop);
            motionDiv->setBounds (timingTop);
            area.removeFromTop (8);

            auto knobRow = area.removeFromTop (126);
            const int knobWidth = 122;
            const int total = knobWidth * 2 + 18;
            knobRow = knobRow.withTrimmedLeft (juce::jmax (0, (knobRow.getWidth() - total) / 2));
            motionRate->setBounds (knobRow.removeFromLeft (knobWidth));
            knobRow.removeFromLeft (18);
            motionSmooth->setBounds (knobRow.removeFromLeft (knobWidth));
        }

        void layoutSequencer (juce::Rectangle<int> area)
        {
            auto top = area.removeFromTop (72);
            seqSteps->setBounds (top.removeFromLeft (190));
            top.removeFromLeft (10);
            seqSynced->setBounds (top.removeFromTop (52));
            area.removeFromTop (8);

            auto timing = area.removeFromTop (116);
            seqRate->setBounds (timing.removeFromLeft (116));
            timing.removeFromLeft (10);
            const auto selector = timing.removeFromTop (58);
            seqRateUnit->setBounds (selector);
            seqDiv->setBounds (selector);
            area.removeFromTop (6);

            auto bottom = area;
            seqSmooth->setBounds (bottom.removeFromLeft (112).removeFromTop (112));
            bottom.removeFromLeft (10);
            seqGrid->setBounds (bottom.removeFromTop (96));
        }

        void layoutMainPanel (juce::Rectangle<int> area)
        {
            auto top = area.removeFromTop (60);
            enableToggle->setBounds (top.removeFromLeft (68).reduced (0, 7));
            top.removeFromLeft (12);
            modeCombo->setBounds (top.removeFromLeft (240));

            area.removeFromTop (10);
            visualizer->setBounds (area.removeFromTop (juce::jlimit (190, 235, (int) (area.getHeight() * 0.48f))));
            area.removeFromTop (12);

            std::vector<LabeledKnob*> knobs;
            knobs.push_back (primaryKnob.get());
            for (auto& knob : secondaryKnobs)
                knobs.push_back (knob.get());

            constexpr int knobWidth = 126;
            constexpr int knobHeight = 138;
            constexpr int gap = 16;
            const int count = (int) knobs.size();
            const int totalWidth = count * knobWidth + juce::jmax (0, count - 1) * gap;
            int x = area.getX() + juce::jmax (0, (area.getWidth() - totalWidth) / 2);
            const int y = area.getY() + juce::jmax (0, (area.getHeight() - knobHeight) / 2);

            for (auto* knob : knobs)
            {
                knob->setBounds (x, y, knobWidth, knobHeight);
                x += knobWidth + gap;
            }
        }

        bool getBool (const juce::String& suffix) const
        {
            if (auto* value = apvts.getRawParameterValue (theSpec.prefix + "_" + suffix))
                return value->load() > 0.5f;
            return false;
        }

        void timerCallback() override
        {
            refreshModContextVisibility();
        }

        void refreshModContextVisibility()
        {
            const int source = modSourceCombo->combo.getSelectedItemIndex();
            const bool isLfo = source == 1;
            const bool isEnv = source == 2;
            const bool isMotion = source == 3;
            const bool isSeq = source == 4;

            const bool lfoTempo = getBool ("lfo_synced");
            const bool motionTempo = getBool ("motion_synced");
            const bool seqTempo = getBool ("seq_synced");

            lfoShape->setVisible (isLfo);
            lfoSynced->setVisible (isLfo);
            lfoRate->setVisible (isLfo && ! lfoTempo);
            lfoRateUnit->setVisible (isLfo && ! lfoTempo);
            lfoDiv->setVisible (isLfo && lfoTempo);

            envAttack->setVisible (isEnv);
            envRelease->setVisible (isEnv);

            motionShape->setVisible (isMotion);
            motionMode->setVisible (isMotion);
            motionSynced->setVisible (isMotion);
            motionRate->setVisible (isMotion && ! motionTempo);
            motionRateUnit->setVisible (isMotion && ! motionTempo);
            motionDiv->setVisible (isMotion && motionTempo);
            motionSmooth->setVisible (isMotion);

            seqSteps->setVisible (isSeq);
            seqSynced->setVisible (isSeq);
            seqRate->setVisible (isSeq && ! seqTempo);
            seqRateUnit->setVisible (isSeq && ! seqTempo);
            seqDiv->setVisible (isSeq && seqTempo);
            seqSmooth->setVisible (isSeq);
            seqGrid->setVisible (isSeq);
            if (isSeq)
                seqGrid->setNumSteps ((int) apvts.getRawParameterValue (theSpec.prefix + "_seq_numsteps")->load());

            visualizer->setActive (source != 0);
        }

        juce::AudioProcessorValueTreeState& apvts;
        EffectChain& chain;
        EffectPanelSpec theSpec;
        EffectId id;

        std::unique_ptr<LabeledToggle> enableToggle;
        std::unique_ptr<LabeledCombo> modeCombo;
        std::unique_ptr<LabeledKnob> primaryKnob;
        std::vector<std::unique_ptr<LabeledKnob>> secondaryKnobs;
        std::unique_ptr<ModVisualizer> visualizer;

        std::unique_ptr<LabeledCombo> modSourceCombo;
        std::unique_ptr<LabeledKnob> modDepthKnob;

        std::unique_ptr<LabeledCombo> lfoShape, lfoRateUnit, lfoDiv;
        std::unique_ptr<LabeledToggle> lfoSynced;
        std::unique_ptr<LabeledKnob> lfoRate;

        std::unique_ptr<LabeledKnob> envAttack, envRelease;

        std::unique_ptr<LabeledCombo> motionShape, motionMode, motionRateUnit, motionDiv;
        std::unique_ptr<LabeledToggle> motionSynced;
        std::unique_ptr<LabeledKnob> motionRate, motionSmooth;

        std::unique_ptr<LabeledKnob> seqSteps, seqRate, seqSmooth;
        std::unique_ptr<LabeledToggle> seqSynced;
        std::unique_ptr<LabeledCombo> seqRateUnit, seqDiv;
        std::unique_ptr<StepBarGrid> seqGrid;
    };
}
