#pragma once
#include "Widgets.h"
#include "DrivePhasePanel.h"
#include <cmath>

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
        EffectPanel (juce::AudioProcessorValueTreeState& state,
                     EffectChain& effectChain,
                     EffectPanelSpec spec,
                     EffectId effectId)
            : apvts (state), chain (effectChain), theSpec (std::move (spec)), id (effectId)
        {
            enableToggle = std::make_unique<LabeledToggle> (apvts, theSpec.prefix + "_enabled", "ON");
            modeCombo = std::make_unique<LabeledCombo> (apvts, theSpec.modeParamId, "MODE");
            primaryKnob = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_base", "AMOUNT", theSpec.accent);
            primaryKnob->setModulationDisplay (
                &chain.uiModValue[(size_t) id],
                apvts.getRawParameterValue (theSpec.prefix + "_moddepth"),
                apvts.getRawParameterValue (theSpec.prefix + "_modsource"));
            addAndMakeVisible (*enableToggle);
            addAndMakeVisible (*modeCombo);
            addAndMakeVisible (*primaryKnob);

            for (const auto& secondary : theSpec.secondaryKnobs)
            {
                auto knob = std::make_unique<LabeledKnob> (apvts, secondary.first, secondary.second, theSpec.accent);
                addAndMakeVisible (*knob);
                secondaryKnobs.push_back (std::move (knob));
            }

            visualizer = std::make_unique<ModVisualizer> (
                chain.uiModValue[(size_t) id],
                chain.uiInputLevel[(size_t) id],
                chain.uiOutputLevel[(size_t) id],
                chain.uiSignalEpoch,
                theSpec.accent);
            addAndMakeVisible (*visualizer);

            modSourceCombo = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_modsource", "MOD SOURCE");
            modDepthKnob = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_moddepth", "DEPTH", theSpec.accent);
            addAndMakeVisible (*modSourceCombo);
            addAndMakeVisible (*modDepthKnob);

            modTargetLabel.setJustificationType (juce::Justification::centredLeft);
            modTargetLabel.setColour (juce::Label::textColourId, theSpec.accent);
            modTargetLabel.setFont (FontBank::font (10.5f, true));
            modTargetLabel.setMinimumHorizontalScale (0.78f);
            addAndMakeVisible (modTargetLabel);

            lfoShape = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_lfo_shape", "SHAPE");
            lfoSynced = std::make_unique<LabeledToggle> (apvts, theSpec.prefix + "_lfo_synced", "TEMPO SYNC");
            lfoRate = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_lfo_rate", "RATE", theSpec.accent);
            lfoRateUnit = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_lfo_rateunit", "UNIT");
            lfoDiv = std::make_unique<LabeledCombo> (apvts, theSpec.prefix + "_lfo_div", "DIVISION");
            addComponents ({ lfoShape.get(), lfoSynced.get(), lfoRate.get(), lfoRateUnit.get(), lfoDiv.get() });

            envAttack = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_env_attack", "ATTACK", theSpec.accent);
            envRelease = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_env_release", "RELEASE", theSpec.accent);
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
            seqSmooth = std::make_unique<LabeledKnob> (apvts, theSpec.prefix + "_seq_smooth", "SMOOTH", theSpec.accent);
            addComponents ({ seqSteps.get(), seqSynced.get(), seqRate.get(), seqRateUnit.get(), seqDiv.get(), seqSmooth.get() });

            seqGrid = std::make_unique<StepBarGrid> (apvts, theSpec.prefix + "_seq_step", theSpec.accent);
            seqGrid->setCurrentStepProvider ([this] { return chain.slots[(size_t) id].mod.seq.getCurrentStepIndex(); });
            addAndMakeVisible (*seqGrid);

            if (id == EffectId::Space)
            {
                spaceDelaySync = std::make_unique<LabeledToggle> (apvts, "space_delay_synced", "TEMPO SYNC");
                spaceDelayUnit = std::make_unique<LabeledCombo> (apvts, "space_delay_rateunit", "TIME UNIT");
                spaceDelayDivision = std::make_unique<LabeledCombo> (apvts, "space_delay_div", "DELAY TIME");
                addComponents ({ spaceDelaySync.get(), spaceDelayUnit.get(), spaceDelayDivision.get() });
            }

            if (id == EffectId::Filter)
            {
                filterSlope = std::make_unique<LabeledCombo> (apvts, "filter_slope", "SLOPE");
                addAndMakeVisible (*filterSlope);
                primaryKnob->setDisplayFunctions (
                    [] (double percent)
                    {
                        return ValueFormatting::frequencyHz (
                            filterHzFromPercent (percent), false);
                    },
                    [] (double percent)
                    {
                        return ValueFormatting::frequencyHz (
                            filterHzFromPercent (percent), true);
                    },
                    [] (const juce::String& text)
                    {
                        return percentFromFilterHz (
                            ValueFormatting::parseEngineeringValue (text));
                    });
                primaryKnob->setLabelText ("CUTOFF");
                primaryKnob->setTooltipText ("Filter cutoff frequency - type a value in Hz");
            }

            if (id == EffectId::Drive)
            {
                driveQuality =
                    std::make_unique<LabeledCombo> (
                        apvts,
                        "drive_quality",
                        "OVERSAMPLING");
                drivePostClip =
                    std::make_unique<LabeledCombo> (
                        apvts,
                        "drive_postclip",
                        "POST");

                driveQuality->combo.setTooltip (
                    "Eco is zero-latency. 2x and 4x use JUCE IIR oversampling and report their latency to the DAW.");
                drivePostClip->combo.setTooltip (
                    "True Peak forces 4x oversampling and applies an inter-sample peak guard.");

                drivePhasePanel =
                    std::make_unique<DrivePhasePanel> (
                        apvts);

                addAndMakeVisible (*driveQuality);
                addAndMakeVisible (*drivePostClip);
                addAndMakeVisible (*drivePhasePanel);
            }

            applyStaticContext();
            startTimerHz (20);
            refreshModContextVisibility();
            refreshEffectContext (true);
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

        static double filterHzFromPercent (double percent)
        {
            return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 100.0, percent) / 100.0);
        }

        static double percentFromFilterHz (double hz)
        {
            const double clamped = juce::jlimit (20.0, 20000.0, hz);
            return 100.0 * std::log (clamped / 20.0) / std::log (1000.0);
        }
        static double widthCrossoverHzFromPercent (double percent)
        {
            return juce::jmap (
                juce::jlimit (0.0, 100.0, percent),
                0.0,
                100.0,
                80.0,
                400.0);
        }

        static double percentFromWidthCrossoverHz (double hz)
        {
            return juce::jmap (
                juce::jlimit (80.0, 400.0, hz),
                80.0,
                400.0,
                0.0,
                100.0);
        }


        static double delayRateHzFromPercent (double percent)
        {
            return 0.1 * std::pow (200.0, juce::jlimit (0.0, 100.0, percent) / 100.0);
        }

        static double percentFromDelayRateHz (double hz)
        {
            const double clamped = juce::jlimit (0.1, 20.0, hz);
            return 100.0 * std::log (clamped / 0.1) / std::log (200.0);
        }

        static bool isDelayMode (int modeIndex)
        {
            return modeIndex == 2 || modeIndex == 3 || modeIndex == 5;
        }

        void applyStaticContext()
        {
            switch (id)
            {
                case EffectId::Drive:
                    primaryKnob->setLabelText ("DRIVE");
                    primaryKnob->setTooltipText (
                        "Global distortion amount");
                    break;

                case EffectId::Pan:
                    primaryKnob->setLabelText ("PAN");
                    primaryKnob->setTooltipText (
                        "Stereo position: 50L through C to 50R");
                    primaryKnob->setDisplayFunctions (
                        [] (double value)
                        {
                            return ValueFormatting::pan (
                                value,
                                false);
                        },
                        [] (double value)
                        {
                            return ValueFormatting::pan (
                                value,
                                true);
                        },
                        [] (const juce::String& text)
                        {
                            return ValueFormatting::parsePan (
                                text);
                        });
                    break;

                case EffectId::Volume:
                    primaryKnob->setTooltipText (
                        "Volume amount");
                    break;

                case EffectId::Space:
                    primaryKnob->setTooltipText (
                        "Space wet amount");
                    break;

                case EffectId::Retro:
                    primaryKnob->setTooltipText (
                        "Retro effect amount");
                    break;

                case EffectId::Width:
                    primaryKnob->setLabelText ("WIDTH");
                    primaryKnob->setTooltipText (
                        "Stereo width: 0% mono, 100% original stereo, 200% maximum side signal");
                    primaryKnob->setDisplayFunctions (
                        [] (double value)
                        {
                            return ValueFormatting::percent (
                                value,
                                false);
                        },
                        [] (double value)
                        {
                            return ValueFormatting::percent (
                                value,
                                true);
                        },
                        [] (const juce::String& text)
                        {
                            return juce::jlimit (
                                0.0,
                                200.0,
                                text.getDoubleValue());
                        });

                    if (! secondaryKnobs.empty())
                    {
                        secondaryKnobs[0]->setTooltipText (
                            "Mono-bass crossover frequency");
                        secondaryKnobs[0]->setDisplayFunctions (
                            [] (double percent)
                            {
                                return ValueFormatting::frequencyHz (
                                    widthCrossoverHzFromPercent (
                                        percent),
                                    false);
                            },
                            [] (double percent)
                            {
                                return ValueFormatting::frequencyHz (
                                    widthCrossoverHzFromPercent (
                                        percent),
                                    true);
                            },
                            [] (const juce::String& text)
                            {
                                return percentFromWidthCrossoverHz (
                                    ValueFormatting::parseEngineeringValue (
                                        text));
                            });
                    }
                    break;

                case EffectId::Filter:
                    break;
            }
        }

        static void setRateValueUnit (
            LabeledKnob& knob,
            int unitIndex)
        {
            if (unitIndex == 1)
            {
                knob.setDisplayFunctions (
                    [] (double value)
                    {
                        return ValueFormatting::seconds (
                            value,
                            false);
                    },
                    [] (double value)
                    {
                        return ValueFormatting::seconds (
                            value,
                            true);
                    },
                    [] (const juce::String& text)
                    {
                        return ValueFormatting::parseSeconds (
                            text);
                    });
                return;
            }

            knob.setDisplayFunctions (
                [] (double value)
                {
                    return ValueFormatting::frequencyHz (
                        value,
                        false);
                },
                [] (double value)
                {
                    return ValueFormatting::frequencyHz (
                        value,
                        true);
                },
                [] (const juce::String& text)
                {
                    return ValueFormatting::parseEngineeringValue (
                        text);
                });
        }

        void refreshRateValueUnits()
        {
            const int lfoUnit = lfoRateUnit->combo.getSelectedItemIndex();
            const int motionUnit = motionRateUnit->combo.getSelectedItemIndex();
            const int sequencerUnit = seqRateUnit->combo.getSelectedItemIndex();
            const int key = lfoUnit + motionUnit * 10 + sequencerUnit * 100;

            if (key == lastRateUnitKey)
                return;

            setRateValueUnit (*lfoRate, lfoUnit);
            setRateValueUnit (*motionRate, motionUnit);
            setRateValueUnit (*seqRate, sequencerUnit);
            lastRateUnitKey = key;
        }

        void layoutModulationPanel (
            juce::Rectangle<int> area)
        {
            auto sourceRow = area.removeFromTop (104);
            auto depthArea =
                sourceRow.removeFromRight (112);
            modDepthKnob->setBounds (depthArea);
            sourceRow.removeFromRight (10);
            modSourceCombo->setBounds (
                sourceRow.removeFromTop (60));

            modTargetLabel.setBounds (
                area.removeFromTop (25));
            area.removeFromTop (7);

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
            modeCombo->setBounds (
                top.removeFromLeft (
                    id == EffectId::Drive
                        ? 205
                        : 240));

            if (driveQuality != nullptr)
            {
                top.removeFromLeft (10);
                driveQuality->setBounds (
                    top.removeFromLeft (130));
                top.removeFromLeft (8);
                drivePostClip->setBounds (
                    top.removeFromLeft (145));
            }

            if (filterSlope != nullptr && filterSlope->isVisible())
            {
                top.removeFromLeft (12);
                filterSlope->setBounds (top.removeFromLeft (170));
            }

            if (spaceDelaySync != nullptr && spaceDelaySync->isVisible())
            {
                top.removeFromLeft (12);
                spaceDelaySync->setBounds (top.removeFromLeft (126).reduced (0, 4));
                top.removeFromLeft (8);
                if (spaceDelayUnit->isVisible())
                    spaceDelayUnit->setBounds (top.removeFromLeft (160));
                else if (spaceDelayDivision->isVisible())
                    spaceDelayDivision->setBounds (top.removeFromLeft (160));
            }

            area.removeFromTop (10);
            const auto visualArea =
                area.removeFromTop (
                    juce::jlimit (
                        175,
                        210,
                        (int) (
                            area.getHeight()
                            * 0.43f)));

            visualizer->setBounds (visualArea);

            if (drivePhasePanel != nullptr)
                drivePhasePanel->setBounds (visualArea);

            area.removeFromTop (12);

            std::vector<LabeledKnob*> knobs;
            if (primaryKnob->isVisible())
                knobs.push_back (primaryKnob.get());
            for (auto& knob : secondaryKnobs)
                if (knob->isVisible())
                    knobs.push_back (knob.get());

            constexpr int knobWidth = 126;
            constexpr int knobHeight = 144;
            constexpr int gap = 16;
            const int count = (int) knobs.size();
            const int totalWidth = count * knobWidth + juce::jmax (0, count - 1) * gap;
            int x = area.getX() + juce::jmax (0, (area.getWidth() - totalWidth) / 2);
            const int y = area.getY() + juce::jmax (0, (area.getHeight() - knobHeight) / 3);

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

        int getChoice (const juce::String& parameterId) const
        {
            if (auto* value = apvts.getRawParameterValue (parameterId))
                return (int) value->load();
            return 0;
        }

        void timerCallback() override
        {
            refreshModContextVisibility();
            refreshEffectContext (false);
            refreshModTargetText();
            primaryKnob->slider.repaint();
        }

        void refreshModTargetText()
        {
            const auto summary =
                primaryKnob->getModulationSummary();

            if (modTargetLabel.getText() != summary)
            {
                modTargetLabel.setText (
                    summary,
                    juce::dontSendNotification);
            }

            modTargetLabel.setTooltip (
                "The outer arc on "
                + primaryKnob->getLabelText()
                + " shows the full modulation range. "
                  "The bright dot shows the live value.");
        }

        void refreshEffectContext (bool force)
        {
            int key = (int) id * 1000;

            if (id == EffectId::Drive)
            {
                const int mode =
                    modeCombo->combo
                        .getSelectedItemIndex();
                const bool grooveMode =
                    mode == 7;

                key += mode * 100
                    + getChoice (
                        "drive_quality") * 10
                    + getChoice (
                        "drive_postclip");

                if (! force
                    && key == lastEffectContextKey)
                {
                    return;
                }

                visualizer->setVisible (
                    ! grooveMode);

                if (drivePhasePanel != nullptr)
                {
                    drivePhasePanel->setVisible (
                        grooveMode);
                }

                if (secondaryKnobs.size() >= 4)
                {
                    auto& tone =
                        *secondaryKnobs[0];
                    auto& bias =
                        *secondaryKnobs[1];
                    auto& mixKnob =
                        *secondaryKnobs[2];
                    auto& output =
                        *secondaryKnobs[3];

                    tone.setVisible (
                        ! grooveMode);
                    bias.setVisible (
                        ! grooveMode);
                    mixKnob.setVisible (true);
                    output.setVisible (true);

                    primaryKnob->setLabelText (
                        "DRIVE");
                    primaryKnob->setTooltipText (
                        "Global distortion amount");

                    tone.setLabelText (
                        mode == 4
                            ? "DAMP"
                            : mode == 5
                                ? "FOLD TONE"
                                : mode == 6
                                    ? "PERIOD"
                                    : "TONE");

                    tone.setTooltipText (
                        mode == 4
                            ? "Tape high-frequency damping before saturation"
                            : mode == 5
                                ? "Spectral balance before wavefolding"
                                : mode == 6
                                    ? "Density of the sinusoidal folds"
                                    : "Pre-emphasis balance before saturation");

                    bias.setLabelText ("BIAS");
                    bias.setTooltipText (
                        "Moves the transfer curve away from symmetry to add even harmonics");
                    mixKnob.setLabelText ("MIX");
                    mixKnob.setTooltipText (
                        "Dry and driven signal blend");
                    output.setLabelText ("OUT");
                    output.setTooltipText (
                        "Post-drive output trim");
                }
            }
            else if (id == EffectId::Space)
            {
                const int mode =
                    modeCombo->combo.getSelectedItemIndex();
                const bool delay = isDelayMode (mode);
                const bool synced =
                    getBool ("delay_synced");
                const int unit =
                    getChoice ("space_delay_rateunit");
                key += mode * 10
                    + (synced ? 2 : 0)
                    + unit;

                if (! force
                    && key == lastEffectContextKey)
                {
                    return;
                }

                spaceDelaySync->setVisible (delay);
                spaceDelayUnit->setVisible (
                    delay && ! synced);
                spaceDelayDivision->setVisible (
                    delay && synced);

                if (secondaryKnobs.size() >= 3)
                {
                    auto& timeOrSize =
                        *secondaryKnobs[0];
                    auto& feedbackOrDecay =
                        *secondaryKnobs[1];
                    auto& tone =
                        *secondaryKnobs[2];

                    if (delay)
                    {
                        timeOrSize.setVisible (! synced);
                        feedbackOrDecay.setLabelText (
                            "FEEDBACK");
                        feedbackOrDecay.setTooltipText (
                            "Delay feedback - higher values create more repeats");
                        tone.setLabelText ("TONE");
                        tone.setTooltipText (
                            "Delay colour");

                        if (unit == 0)
                        {
                            timeOrSize.setLabelText ("RATE");
                            timeOrSize.setTooltipText (
                                "Delay repetition rate");
                            timeOrSize.setDisplayFunctions (
                                [] (double percent)
                                {
                                    return ValueFormatting::frequencyHz (
                                        delayRateHzFromPercent (
                                            percent),
                                        false);
                                },
                                [] (double percent)
                                {
                                    return ValueFormatting::frequencyHz (
                                        delayRateHzFromPercent (
                                            percent),
                                        true);
                                },
                                [] (const juce::String& text)
                                {
                                    return percentFromDelayRateHz (
                                        ValueFormatting::parseEngineeringValue (
                                            text));
                                });
                        }
                        else
                        {
                            timeOrSize.setLabelText ("TIME");
                            timeOrSize.setTooltipText (
                                "Time between delay repeats");
                            timeOrSize.setDisplayFunctions (
                                [] (double percent)
                                {
                                    return ValueFormatting::seconds (
                                        1.0
                                        / delayRateHzFromPercent (
                                            percent),
                                        false);
                                },
                                [] (double percent)
                                {
                                    return ValueFormatting::seconds (
                                        1.0
                                        / delayRateHzFromPercent (
                                            percent),
                                        true);
                                },
                                [] (const juce::String& text)
                                {
                                    const double seconds =
                                        juce::jmax (
                                            0.05,
                                            ValueFormatting::parseSeconds (
                                                text));

                                    return percentFromDelayRateHz (
                                        1.0 / seconds);
                                });
                        }
                    }
                    else
                    {
                        timeOrSize.setVisible (true);
                        timeOrSize.setLabelText ("SIZE");
                        timeOrSize.setTooltipText (
                            "Reverb space size");
                        timeOrSize.restoreDefaultFormatter();
                        feedbackOrDecay.setLabelText (
                            "DECAY");
                        feedbackOrDecay.setTooltipText (
                            "Reverb tail length");
                        tone.setLabelText ("TONE");
                        tone.setTooltipText (
                            "Reverb brightness");
                    }
                }
            }
            else if (id == EffectId::Filter)
            {
                const int mode =
                    modeCombo->combo.getSelectedItemIndex();
                key += mode;

                if (! force
                    && key == lastEffectContextKey)
                {
                    return;
                }

                const bool comb = mode == 5;
                filterSlope->setVisible (! comb);

                if (secondaryKnobs.size() >= 2)
                {
                    secondaryKnobs[0]->setLabelText (
                        comb
                            ? "FEEDBACK"
                            : "RESONANCE");
                    secondaryKnobs[0]->setTooltipText (
                        comb
                            ? "Comb-filter feedback"
                            : "Filter resonance around the cutoff frequency");
                    secondaryKnobs[1]->setLabelText (
                        "MIX");
                    secondaryKnobs[1]->setTooltipText (
                        "Filtered signal amount");
                }
            }
            else if (id == EffectId::Retro)
            {
                const int mode =
                    modeCombo->combo.getSelectedItemIndex();
                key += mode * 100000
                    + (int) chain.retro.getSampleRate();

                if (! force
                    && key == lastEffectContextKey)
                {
                    return;
                }

                if (! secondaryKnobs.empty())
                {
                    auto& rate = *secondaryKnobs[0];

                    switch (mode)
                    {
                        case 0:
                            rate.setLabelText (
                                "SAMPLE RATE");
                            rate.setTooltipText (
                                "Effective sample-and-hold rate used by Bitcrush");
                            rate.setDisplayFunctions (
                                [this] (double percent)
                                {
                                    return ValueFormatting::frequencyHz (
                                        chain.retro.bitcrushSampleRateHz (
                                            (float) percent
                                            / 100.0f),
                                        false);
                                },
                                [this] (double percent)
                                {
                                    return ValueFormatting::frequencyHz (
                                        chain.retro.bitcrushSampleRateHz (
                                            (float) percent
                                            / 100.0f),
                                        true);
                                },
                                [this] (const juce::String& text)
                                {
                                    return 100.0
                                        * chain.retro
                                            .bitcrushNormalisedFromSampleRateHz (
                                                (float)
                                                ValueFormatting::parseEngineeringValue (
                                                    text));
                                });
                            break;

                        case 1:
                            rate.setLabelText (
                                "BANDWIDTH");
                            rate.setTooltipText (
                                "Lossy-codec bandwidth");
                            rate.setDisplayFunctions (
                                [] (double percent)
                                {
                                    return ValueFormatting::frequencyHz (
                                        RetroEffect::lossyBandwidthHz (
                                            (float) percent
                                            / 100.0f),
                                        false);
                                },
                                [] (double percent)
                                {
                                    return ValueFormatting::frequencyHz (
                                        RetroEffect::lossyBandwidthHz (
                                            (float) percent
                                            / 100.0f),
                                        true);
                                },
                                [] (const juce::String& text)
                                {
                                    return 100.0
                                        * RetroEffect
                                            ::lossyNormalisedFromBandwidthHz (
                                                (float)
                                                ValueFormatting::parseEngineeringValue (
                                                    text));
                                });
                            break;

                        case 2:
                            rate.setLabelText (
                                "WOW RATE");
                            rate.setTooltipText (
                                "Wow and flutter movement rate");
                            rate.setDisplayFunctions (
                                [] (double percent)
                                {
                                    return ValueFormatting::frequencyHz (
                                        RetroEffect::wearRateHz (
                                            (float) percent
                                            / 100.0f),
                                        false);
                                },
                                [] (double percent)
                                {
                                    return ValueFormatting::frequencyHz (
                                        RetroEffect::wearRateHz (
                                            (float) percent
                                            / 100.0f),
                                        true);
                                },
                                [] (const juce::String& text)
                                {
                                    return 100.0
                                        * RetroEffect
                                            ::wearNormalisedFromRateHz (
                                                (float)
                                                ValueFormatting::parseEngineeringValue (
                                                    text));
                                });
                            break;

                        default:
                            rate.setLabelText (
                                "FILTER");
                            rate.setTooltipText (
                                "E-mu-style output filter cutoff");
                            rate.setDisplayFunctions (
                                [] (double percent)
                                {
                                    return ValueFormatting::frequencyHz (
                                        RetroEffect::emuFilterHz (
                                            (float) percent
                                            / 100.0f),
                                        false);
                                },
                                [] (double percent)
                                {
                                    return ValueFormatting::frequencyHz (
                                        RetroEffect::emuFilterHz (
                                            (float) percent
                                            / 100.0f),
                                        true);
                                },
                                [] (const juce::String& text)
                                {
                                    return 100.0
                                        * RetroEffect
                                            ::emuNormalisedFromFilterHz (
                                                (float)
                                                ValueFormatting::parseEngineeringValue (
                                                    text));
                                });
                            break;
                    }
                }
            }
            else if (! force
                     && key == lastEffectContextKey)
            {
                return;
            }

            lastEffectContextKey = key;
            refreshModTargetText();
            resized();
            repaint();
        }

        void refreshModContextVisibility()
        {
            refreshRateValueUnits();
            const int source = modSourceCombo->combo.getSelectedItemIndex();
            const bool isLfo = source == 1;
            const bool isEnvelope = source == 2;
            const bool isMotion = source == 3;
            const bool isSequencer = source == 4;

            const bool lfoTempo = getBool ("lfo_synced");
            const bool motionTempo = getBool ("motion_synced");
            const bool sequencerTempo = getBool ("seq_synced");

            lfoShape->setVisible (isLfo);
            lfoSynced->setVisible (isLfo);
            lfoRate->setVisible (isLfo && ! lfoTempo);
            lfoRateUnit->setVisible (isLfo && ! lfoTempo);
            lfoDiv->setVisible (isLfo && lfoTempo);

            envAttack->setVisible (isEnvelope);
            envRelease->setVisible (isEnvelope);

            motionShape->setVisible (isMotion);
            motionMode->setVisible (isMotion);
            motionSynced->setVisible (isMotion);
            motionRate->setVisible (isMotion && ! motionTempo);
            motionRateUnit->setVisible (isMotion && ! motionTempo);
            motionDiv->setVisible (isMotion && motionTempo);
            motionSmooth->setVisible (isMotion);

            seqSteps->setVisible (isSequencer);
            seqSynced->setVisible (isSequencer);
            seqRate->setVisible (isSequencer && ! sequencerTempo);
            seqRateUnit->setVisible (isSequencer && ! sequencerTempo);
            seqDiv->setVisible (isSequencer && sequencerTempo);
            seqSmooth->setVisible (isSequencer);
            seqGrid->setVisible (isSequencer);
            if (isSequencer)
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
        juce::Label modTargetLabel;

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

        std::unique_ptr<LabeledToggle> spaceDelaySync;
        std::unique_ptr<LabeledCombo> spaceDelayUnit, spaceDelayDivision;
        std::unique_ptr<LabeledCombo> filterSlope;
        std::unique_ptr<LabeledCombo> driveQuality;
        std::unique_ptr<LabeledCombo> drivePostClip;
        std::unique_ptr<DrivePhasePanel> drivePhasePanel;
        int lastEffectContextKey = -1;
        int lastRateUnitKey = -1;
    };
}
