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
        std::vector<std::pair<juce::String, juce::String>> secondaryKnobs; // paramId -> label
    };

    class EffectPanel : public juce::Component, private juce::Timer
    {
    public:
        EffectPanel (juce::AudioProcessorValueTreeState& s, EffectChain& c, EffectPanelSpec spec, EffectId effectId)
            : apvts (s), chain (c), theSpec (std::move (spec)), id (effectId)
        {
            enableToggle = std::make_unique<LabeledToggle> (apvts, theSpec.prefix + "_enabled", "ON");
            addAndMakeVisible (*enableToggle);

            modeCombo = std::make_unique<LabeledCombo> (apvts, theSpec.modeParamId, "MODE");
            addAndMakeVisible (*modeCombo);

            primaryKnob = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_base", "AMOUNT", theSpec.accent);
            addAndMakeVisible (*primaryKnob);

            for (auto& sk : theSpec.secondaryKnobs)
            {
                auto k = std::make_unique<LabeledKnob> (apvts, sk.first, sk.second, theSpec.accent);
                addAndMakeVisible (*k);
                secondaryKnobs.push_back (std::move (k));
            }

            visualizer = std::make_unique<ModVisualizer> (chain.uiModValue[(size_t) id], theSpec.accent);
            addAndMakeVisible (*visualizer);

            // --- left modulation panel ---
            modSourceCombo = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_modsource", "MOD SOURCE");
            addAndMakeVisible (*modSourceCombo);
            modDepthKnob = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_moddepth", "DEPTH", theSpec.accent);
            addAndMakeVisible (*modDepthKnob);

            lfoShape = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_lfo_shape", "SHAPE");
            lfoSynced = std::make_unique<LabeledToggle> (apvts, theSpec.prefix + "_lfo_synced", "SYNC");
            lfoRate = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_lfo_rate", "RATE Hz", theSpec.accent);
            lfoDiv = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_lfo_div", "DIVISION");
            for (auto* c2 : { (juce::Component*) lfoShape.get(), (juce::Component*) lfoSynced.get(),
                              (juce::Component*) lfoRate.get(), (juce::Component*) lfoDiv.get() })
                addAndMakeVisible (c2);

            envAttack = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_env_attack", "ATTACK", theSpec.accent);
            envRelease = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_env_release", "RELEASE", theSpec.accent);
            addAndMakeVisible (*envAttack); addAndMakeVisible (*envRelease);

            motionShape = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_motion_shape", "SHAPE");
            motionMode = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_motion_mode", "MODE");
            motionSynced = std::make_unique<LabeledToggle> (apvts, theSpec.prefix + "_motion_synced", "SYNC");
            motionRate = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_motion_rate", "RATE Hz", theSpec.accent);
            motionDiv = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_motion_div", "DIVISION");
            motionSmooth = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_motion_smooth", "SMOOTH", theSpec.accent);
            for (auto* c2 : { (juce::Component*) motionShape.get(), (juce::Component*) motionMode.get(), (juce::Component*) motionSynced.get(),
                              (juce::Component*) motionRate.get(), (juce::Component*) motionDiv.get(), (juce::Component*) motionSmooth.get() })
                addAndMakeVisible (c2);

            seqSteps = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_seq_numsteps", "STEPS", theSpec.accent);
            seqSteps->slider.setSliderStyle (juce::Slider::LinearHorizontal);
            seqSynced = std::make_unique<LabeledToggle> (apvts, theSpec.prefix + "_seq_synced", "SYNC");
            seqRate = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_seq_rate", "RATE Hz", theSpec.accent);
            seqDiv = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_seq_div", "DIVISION");
            seqSmooth = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_seq_smooth", "SMOOTH", theSpec.accent);
            for (auto* c2 : { (juce::Component*) seqSteps.get(), (juce::Component*) seqSynced.get(),
                              (juce::Component*) seqRate.get(), (juce::Component*) seqDiv.get(), (juce::Component*) seqSmooth.get() })
                addAndMakeVisible (c2);

            seqGrid = std::make_unique<StepBarGrid> (apvts, theSpec.prefix + "_seq_step", theSpec.accent);
            seqGrid->setCurrentStepProvider ([this] { return chain.slots[(size_t) id].mod.seq.getCurrentStepIndex(); });
            addAndMakeVisible (*seqGrid);

            startTimerHz (12);
            refreshModContextVisibility();
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (10);

            auto left = b.removeFromLeft (juce::jmax (170, (int) (b.getWidth() * 0.28f)));
            b.removeFromLeft (10);
            layoutLeftPanel (left);
            layoutMainPanel (b);
        }

    private:
        void layoutLeftPanel (juce::Rectangle<int> left)
        {
            auto top = left.removeFromTop (76);
            modDepthKnob->setBounds (top.removeFromRight (70));
            top.removeFromRight (8);
            modSourceCombo->setBounds (top.removeFromTop (48));
            left.removeFromTop (8);

            auto ctx = left;
            int rowH = 46;
            auto row = [&ctx, rowH]() { return ctx.removeFromTop (rowH); };

            // LFO
            {
                auto r1 = row(); lfoShape->setBounds (r1.removeFromLeft (r1.getWidth() / 2)); lfoSynced->setBounds (r1);
                auto r2 = row(); lfoRate->setBounds (r2.removeFromLeft (r2.getWidth() / 2)); lfoDiv->setBounds (r2);
            }
            // Env
            {
                auto r1 = row(); envAttack->setBounds (r1.removeFromLeft (r1.getWidth() / 2)); envRelease->setBounds (r1);
            }
            // Motion
            {
                auto r1 = row(); motionShape->setBounds (r1.removeFromLeft (r1.getWidth() / 2)); motionMode->setBounds (r1);
                auto r2 = row(); motionSynced->setBounds (r2.removeFromLeft (r2.getWidth() / 3));
                motionRate->setBounds (r2.removeFromLeft (r2.getWidth() / 2)); motionDiv->setBounds (r2);
                auto r3 = row(); motionSmooth->setBounds (r3.removeFromLeft (r3.getWidth() / 2));
            }
            // Sequencer
            {
                auto r1 = row(); seqSteps->setBounds (r1.removeFromLeft (r1.getWidth() / 2)); seqSynced->setBounds (r1);
                auto r2 = row(); seqRate->setBounds (r2.removeFromLeft (r2.getWidth() / 2)); seqDiv->setBounds (r2);
                auto r3 = row(); seqSmooth->setBounds (r3.removeFromLeft (r3.getWidth() / 2));
                ctx.removeFromTop (4);
                seqGrid->setBounds (ctx.removeFromTop (70));
            }
        }

        void layoutMainPanel (juce::Rectangle<int> b)
        {
            auto top = b.removeFromTop (34);
            enableToggle->setBounds (top.removeFromLeft (50));
            top.removeFromLeft (10);
            modeCombo->setBounds (top.removeFromLeft (160));

            b.removeFromTop (10);
            visualizer->setBounds (b.removeFromTop (juce::jmax (90, (int) (b.getHeight() * 0.42f))));
            b.removeFromTop (14);

            auto knobRow = b.removeFromTop (110);
            int n = 1 + (int) secondaryKnobs.size();
            int w = knobRow.getWidth() / juce::jmax (1, n);
            primaryKnob->setBounds (knobRow.removeFromLeft (w));
            for (auto& k : secondaryKnobs)
                k->setBounds (knobRow.removeFromLeft (w));
        }

        void timerCallback() override { refreshModContextVisibility(); }

        void refreshModContextVisibility()
        {
            int src = modSourceCombo->combo.getSelectedItemIndex();
            bool isLfo = src == 1, isEnv = src == 2, isMotion = src == 3, isSeq = src == 4;
            bool active = src != 0;

            for (auto* c2 : { (juce::Component*) lfoShape.get(), (juce::Component*) lfoSynced.get(),
                              (juce::Component*) lfoRate.get(), (juce::Component*) lfoDiv.get() }) c2->setVisible (isLfo);
            envAttack->setVisible (isEnv); envRelease->setVisible (isEnv);
            for (auto* c2 : { (juce::Component*) motionShape.get(), (juce::Component*) motionMode.get(), (juce::Component*) motionSynced.get(),
                              (juce::Component*) motionRate.get(), (juce::Component*) motionDiv.get(), (juce::Component*) motionSmooth.get() }) c2->setVisible (isMotion);
            for (auto* c2 : { (juce::Component*) seqSteps.get(), (juce::Component*) seqSynced.get(),
                              (juce::Component*) seqRate.get(), (juce::Component*) seqDiv.get(), (juce::Component*) seqSmooth.get() }) c2->setVisible (isSeq);
            seqGrid->setVisible (isSeq);
            if (isSeq) seqGrid->setNumSteps ((int) apvts.getRawParameterValue (theSpec.prefix + "_seq_numsteps")->load());

            visualizer->setActive (active);
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

        std::unique_ptr<LabeledCombo> lfoShape, lfoDiv;
        std::unique_ptr<LabeledToggle> lfoSynced;
        std::unique_ptr<LabeledKnob> lfoRate;

        std::unique_ptr<LabeledKnob> envAttack, envRelease;

        std::unique_ptr<LabeledCombo> motionShape, motionMode, motionDiv;
        std::unique_ptr<LabeledToggle> motionSynced;
        std::unique_ptr<LabeledKnob> motionRate, motionSmooth;

        std::unique_ptr<LabeledKnob> seqSteps, seqRate, seqSmooth;
        std::unique_ptr<LabeledToggle> seqSynced;
        std::unique_ptr<LabeledCombo> seqDiv;
        std::unique_ptr<StepBarGrid> seqGrid;
    };
}
