#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
if not (ROOT / "MotionFX_source").is_dir():
    # Allow the updater to live in a small extracted folder under the repo root.
    if (ROOT.parent / "MotionFX_source").is_dir():
        ROOT = ROOT.parent
    else:
        print("ERROR: place apply_block4.py at the repository root, next to MotionFX_source and .github")
        sys.exit(1)

changed: list[str] = []


def read(rel: str) -> str:
    path = ROOT / rel
    if not path.exists():
        raise RuntimeError(f"Missing required file: {rel}")
    return path.read_text(encoding="utf-8")


def write(rel: str, text: str) -> None:
    path = ROOT / rel
    path.write_text(text, encoding="utf-8", newline="\n")
    if rel not in changed:
        changed.append(rel)


def replace_once(rel: str, old: str, new: str) -> None:
    text = read(rel)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"Expected exactly one match in {rel}, found {count}: {old[:100]!r}")
    write(rel, text.replace(old, new, 1))


def regex_once(rel: str, pattern: str, replacement: str, flags: int = re.S) -> None:
    text = read(rel)
    new_text, count = re.subn(pattern, replacement, text, count=1, flags=flags)
    if count != 1:
        raise RuntimeError(f"Expected exactly one regex match in {rel}, found {count}: {pattern[:100]!r}")
    write(rel, new_text)


try:
    # -------------------------------------------------------------------------
    # Version and real CTest registration.
    # -------------------------------------------------------------------------
    replace_once(
        "MotionFX_source/CMakeLists.txt",
        "project(MotionFX VERSION 0.3.0)",
        "project(MotionFX VERSION 0.4.0)",
    )

    cmake = read("MotionFX_source/CMakeLists.txt")
    if "add_test(NAME MotionFXAudioIntegrity" not in cmake:
        cmake += """

# Register the executables with CTest so CI actually executes them.
include(CTest)
enable_testing()
add_test(NAME MotionFXAudioIntegrity COMMAND MotionFXAudioTest)
add_test(NAME MotionFXGuiResize COMMAND MotionFXGuiTest)
set_tests_properties(MotionFXAudioIntegrity PROPERTIES TIMEOUT 300)
set_tests_properties(MotionFXGuiResize PROPERTIES TIMEOUT 90)
"""
        write("MotionFX_source/CMakeLists.txt", cmake)

    # -------------------------------------------------------------------------
    # Modulation time divisions. Existing indices 0..9 stay untouched so old
    # presets/sessions remain compatible; new dotted/triplet choices are appended.
    # -------------------------------------------------------------------------
    replace_once(
        "MotionFX_source/Source/DSP/Modulation.h",
        """    enum class SyncDiv
    {
        d4Bar, d2Bar, d1Bar, d1_2, d1_4, d1_8, d1_8T, d1_16, d1_16T, d1_32
    };
""",
        """    enum class SyncDiv
    {
        // Keep the first ten values stable for preset/session compatibility.
        d4Bar, d2Bar, d1Bar, d1_2, d1_4, d1_8, d1_8T, d1_16, d1_16T, d1_32,
        d1_2D, d1_2T, d1_4D, d1_4T, d1_8D, d1_16D, d1_32D, d1_32T
    };

    static constexpr int syncDivCount = 18;
""",
    )

    replace_once(
        "MotionFX_source/Source/DSP/Modulation.h",
        """            case SyncDiv::d1_32:  return 0.125;
        }
""",
        """            case SyncDiv::d1_32:  return 0.125;
            case SyncDiv::d1_2D:  return 3.0;
            case SyncDiv::d1_2T:  return 4.0 / 3.0;
            case SyncDiv::d1_4D:  return 1.5;
            case SyncDiv::d1_4T:  return 2.0 / 3.0;
            case SyncDiv::d1_8D:  return 0.75;
            case SyncDiv::d1_16D: return 0.375;
            case SyncDiv::d1_32D: return 0.1875;
            case SyncDiv::d1_32T: return 1.0 / 12.0;
        }
""",
    )

    replace_once(
        "MotionFX_source/Source/DSP/Modulation.h",
        """            case SyncDiv::d1_32:  return "1/32";
        }
""",
        """            case SyncDiv::d1_32:  return "1/32";
            case SyncDiv::d1_2D:  return "1/2 D";
            case SyncDiv::d1_2T:  return "1/2 T";
            case SyncDiv::d1_4D:  return "1/4 D";
            case SyncDiv::d1_4T:  return "1/4 T";
            case SyncDiv::d1_8D:  return "1/8 D";
            case SyncDiv::d1_16D: return "1/16 D";
            case SyncDiv::d1_32D: return "1/32 D";
            case SyncDiv::d1_32T: return "1/32 T";
        }
""",
    )

    # -------------------------------------------------------------------------
    # Parameters: extended divisions and Hz/seconds selection for every free-rate
    # modulator. The existing sync booleans and parameter IDs stay compatible.
    # -------------------------------------------------------------------------
    replace_once(
        "MotionFX_source/Source/Parameters.h",
        """        return { "4 Bar", "2 Bar", "1 Bar", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    }

    inline juce::StringArray modSourceChoices()""",
        """        return { "4 Bar", "2 Bar", "1 Bar", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32",
                 "1/2 D", "1/2 T", "1/4 D", "1/4 T", "1/8 D", "1/16 D", "1/32 D", "1/32 T" };
    }

    inline juce::StringArray rateUnitChoices() { return { "Hz", "Seconds" }; }
    inline juce::StringArray modSourceChoices()""",
    )

    replace_once(
        "MotionFX_source/Source/Parameters.h",
        """        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "lfo_rate"), 1), prefix + " LFO Rate Hz",
            juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.35f), 1.0f));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
""",
        """        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "lfo_rate"), 1), prefix + " LFO Rate",
            juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.35f), 1.0f));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "lfo_rateunit"), 1), prefix + " LFO Rate Unit", rateUnitChoices(), 0));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
""",
    )

    replace_once(
        "MotionFX_source/Source/Parameters.h",
        """        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "motion_rate"), 1), prefix + " Motion Rate Hz",
            juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.35f), 0.5f));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
""",
        """        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "motion_rate"), 1), prefix + " Motion Rate",
            juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.35f), 0.5f));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "motion_rateunit"), 1), prefix + " Motion Rate Unit", rateUnitChoices(), 0));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
""",
    )

    replace_once(
        "MotionFX_source/Source/Parameters.h",
        """        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "seq_rate"), 1), prefix + " Seq Rate Hz",
            juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.35f), 2.0f));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
""",
        """        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "seq_rate"), 1), prefix + " Seq Rate",
            juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.35f), 2.0f));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "seq_rateunit"), 1), prefix + " Seq Rate Unit", rateUnitChoices(), 0));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
""",
    )

    # -------------------------------------------------------------------------
    # Processor conversion: free-rate values can represent Hz or cycle seconds.
    # -------------------------------------------------------------------------
    replace_once(
        "MotionFX_source/Source/PluginProcessor.cpp",
        "static mfx::SyncDiv toDiv (int idx) noexcept { return (mfx::SyncDiv) juce::jlimit (0, 9, idx); }",
        """static mfx::SyncDiv toDiv (int idx) noexcept
{
    return (mfx::SyncDiv) juce::jlimit (0, mfx::syncDivCount - 1, idx);
}

static float freeRateToHz (float value, int unit) noexcept
{
    // Unit 0 = cycles per second, unit 1 = seconds per cycle.
    return unit == 1 ? 1.0f / juce::jmax (0.01f, value) : value;
}""",
    )

    replace_once(
        "MotionFX_source/Source/PluginProcessor.cpp",
        """    mod.lfo.setParams ((mfx::LfoShape) (int) raw ("lfo_shape"), raw ("lfo_synced") > 0.5f,
                        raw ("lfo_rate"), toDiv ((int) raw ("lfo_div")), false);
""",
        """    mod.lfo.setParams ((mfx::LfoShape) (int) raw ("lfo_shape"), raw ("lfo_synced") > 0.5f,
                        freeRateToHz (raw ("lfo_rate"), (int) raw ("lfo_rateunit")),
                        toDiv ((int) raw ("lfo_div")), false);
""",
    )

    replace_once(
        "MotionFX_source/Source/PluginProcessor.cpp",
        """    mod.motion.setParams ((mfx::MotionMode) (int) raw ("motion_mode"), raw ("motion_synced") > 0.5f,
                           raw ("motion_rate"), toDiv ((int) raw ("motion_div")), raw ("motion_smooth") / 100.0f);
""",
        """    mod.motion.setParams ((mfx::MotionMode) (int) raw ("motion_mode"), raw ("motion_synced") > 0.5f,
                           freeRateToHz (raw ("motion_rate"), (int) raw ("motion_rateunit")),
                           toDiv ((int) raw ("motion_div")), raw ("motion_smooth") / 100.0f);
""",
    )

    replace_once(
        "MotionFX_source/Source/PluginProcessor.cpp",
        """    mod.seq.setParams (raw ("seq_synced") > 0.5f, raw ("seq_rate"), toDiv ((int) raw ("seq_div")), raw ("seq_smooth"));
""",
        """    mod.seq.setParams (raw ("seq_synced") > 0.5f,
                       freeRateToHz (raw ("seq_rate"), (int) raw ("seq_rateunit")),
                       toDiv ((int) raw ("seq_div")), raw ("seq_smooth"));
""",
    )

    # -------------------------------------------------------------------------
    # Knob formatting and sizing. Force two decimals for float controls, zero for
    # integer controls, and make direct text entry deterministic.
    # -------------------------------------------------------------------------
    widgets_path = "MotionFX_source/Source/GUI/Widgets.h"
    widgets = read(widgets_path)
    knob_pattern = r"    class LabeledKnob : public juce::Component\n    \{.*?\n    \};\n\n    //==============================================================================\n    class LabeledCombo"
    new_knob = r'''    class LabeledKnob : public juce::Component
    {
    public:
        LabeledKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
                     const juce::String& labelText, juce::Colour accent)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 20);
            slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
            slider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
            slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            slider.setTooltip (labelText + " - double-click the value to type it");
            addAndMakeVisible (slider);

            label.setText (labelText, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setColour (juce::Label::textColourId, Palette::textDim);
            addAndMakeVisible (label);

            auto* parameter = apvts.getParameter (paramId);
            const int decimals = dynamic_cast<juce::AudioParameterInt*> (parameter) != nullptr ? 0 : 2;
            slider.setNumDecimalPlacesToDisplay (decimals);
            slider.textFromValueFunction = [decimals] (double value)
            {
                return juce::String (value, decimals);
            };
            slider.valueFromTextFunction = [] (const juce::String& text)
            {
                return text.getDoubleValue();
            };

            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            label.setBounds (b.removeFromBottom (18));
            slider.setBounds (b);
        }

        EditableSlider slider;

    private:
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    //==============================================================================
    class LabeledCombo'''
    widgets_new, count = re.subn(knob_pattern, new_knob, widgets, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError("Could not replace LabeledKnob in Widgets.h")
    write(widgets_path, widgets_new)

    # -------------------------------------------------------------------------
    # Look and feel: the old rotary inset used width only, which made knobs almost
    # disappear in wide cells. Use the smaller dimension and larger readable fonts.
    # -------------------------------------------------------------------------
    replace_once(
        "MotionFX_source/Source/GUI/LookAndFeel.h",
        "auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (width * 0.08f);",
        "auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (juce::jmin (width, height) * 0.08f);",
    )

    replace_once(
        "MotionFX_source/Source/GUI/LookAndFeel.h",
        """        void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool, bool) override {}
""",
        """        juce::Font getComboBoxFont (juce::ComboBox& box) override
        {
            return juce::Font (juce::FontOptions (juce::jlimit (13.0f, 17.0f, box.getHeight() * 0.42f)));
        }

        juce::Font getPopupMenuFont() override
        {
            return juce::Font (juce::FontOptions (14.0f));
        }

        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        {
            return juce::Font (juce::FontOptions (juce::jlimit (12.0f, 16.0f, buttonHeight * 0.42f)));
        }

        void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool, bool) override {}
""",
    )

    # -------------------------------------------------------------------------
    # Full ergonomic EffectPanel replacement. Source-specific controls now share
    # one contextual area instead of being stacked far apart, so every visible
    # knob and selector can use a consistent readable size.
    # -------------------------------------------------------------------------
    effect_panel = r'''#pragma once
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
'''
    current_effect = read("MotionFX_source/Source/GUI/EffectPanel.h")
    if "class EffectPanel" not in current_effect or "_modsource" not in current_effect:
        raise RuntimeError("EffectPanel.h is not the expected MotionFX Block 3 version")
    write("MotionFX_source/Source/GUI/EffectPanel.h", effect_panel)

    # -------------------------------------------------------------------------
    # Stutter DSP: Repeat 1/4, 1/8, 1/16 and 1/32 are now absolute musical
    # lengths, rather than repeated fractions of the already-short grid cell.
    # -------------------------------------------------------------------------
    stutter_engine = r'''#pragma once
#include "Modulation.h"
#include <array>

namespace mfx
{
    enum class StepAction
    {
        Off, Repeat4, Repeat8, Repeat16, Repeat32,
        Reverse, TapeStopDown, TapeStopUp, PitchUp, PitchDown, Gate
    };

    class StutterEngine
    {
    public:
        static constexpr int maxSteps = 32;

        void prepare (double sr) noexcept
        {
            sampleRate = sr;
            const int cap = (int) (8.0 * sr);
            for (auto& buffer : circBuf) buffer.assign ((size_t) cap, 0.0f);
            for (auto& buffer : snippetBuf) buffer.assign ((size_t) cap, 0.0f);
            capacity = cap;
            reset();
        }

        void reset() noexcept
        {
            writePos = 0;
            validHistorySamples = 0;
            lastStepIndex = -1;
            sampleCounterInStep = 0;
            playPos = 0.0;
            rate = 1.0;
            loopSnippetLen = 1;
            loopCounter = 0;
            currentAction = StepAction::Off;
            for (auto& buffer : circBuf)
                std::fill (buffer.begin(), buffer.end(), 0.0f);
        }

        void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }
        void setPattern (int n, SyncDiv d) noexcept { numSteps = juce::jlimit (1, maxSteps, n); division = d; }
        void setStepAction (int i, StepAction action) noexcept { if (i >= 0 && i < maxSteps) pattern[(size_t) i] = action; }
        StepAction getStepAction (int i) const noexcept { return (i >= 0 && i < maxSteps) ? pattern[(size_t) i] : StepAction::Off; }
        void setMix (float newMix) noexcept { mix = juce::jlimit (0.0f, 1.0f, newMix); }

        void processBlock (juce::AudioBuffer<float>& buffer, const TransportInfo& transport) noexcept
        {
            if (! enabled || buffer.getNumChannels() < 2)
                return;

            const double beatsPerStep = syncDivToBeats (division);
            const double secondsPerBeat = 60.0 / juce::jmax (1.0, transport.bpm);
            const double stepLengthSamplesExact = beatsPerStep * secondsPerBeat * sampleRate;
            const int stepLengthSamples = juce::jmax (4, (int) std::round (stepLengthSamplesExact));
            const int actionFadeLength = juce::jmin (stepLengthSamples / 4, (int) (0.003 * sampleRate) + 1);

            double continuousStepPosition = transport.ppqPosition / beatsPerStep;
            continuousStepPosition = std::fmod (continuousStepPosition, (double) numSteps);
            if (continuousStepPosition < 0.0)
                continuousStepPosition += numSteps;

            auto* left = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);
            const int numSamples = buffer.getNumSamples();

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const double instantaneousPosition = continuousStepPosition
                    + (double) sample * (1.0 / stepLengthSamplesExact);
                int stepIndex = (int) std::floor (instantaneousPosition) % numSteps;
                if (stepIndex < 0)
                    stepIndex += numSteps;

                if (stepIndex != lastStepIndex)
                {
                    lastStepIndex = stepIndex;
                    sampleCounterInStep = 0;
                    currentAction = pattern[(size_t) stepIndex];
                    beginStep (currentAction, stepLengthSamples, transport.bpm);
                }

                const float liveLeft = left[sample];
                const float liveRight = right[sample];
                circBuf[0][(size_t) writePos] = liveLeft;
                circBuf[1][(size_t) writePos] = liveRight;

                float processedLeft = liveLeft;
                float processedRight = liveRight;
                const bool actionIsOff = currentAction == StepAction::Off;

                if (! actionIsOff)
                    computeSample (processedLeft, processedRight);

                float actionFade = 1.0f;
                if (! actionIsOff)
                {
                    if (sampleCounterInStep < actionFadeLength)
                        actionFade = (float) sampleCounterInStep / (float) juce::jmax (1, actionFadeLength);
                    else if (sampleCounterInStep > stepLengthSamples - actionFadeLength)
                        actionFade = juce::jmax (0.0f,
                            (float) (stepLengthSamples - sampleCounterInStep) / (float) juce::jmax (1, actionFadeLength));
                }

                const float effectedLeft = actionIsOff ? liveLeft : juce::jmap (actionFade, liveLeft, processedLeft);
                const float effectedRight = actionIsOff ? liveRight : juce::jmap (actionFade, liveRight, processedRight);
                left[sample] = flushDenorm (juce::jmap (mix, liveLeft, effectedLeft));
                right[sample] = flushDenorm (juce::jmap (mix, liveRight, effectedRight));

                writePos = (writePos + 1) % capacity;
                validHistorySamples = juce::jmin (capacity, validHistorySamples + 1);
                ++sampleCounterInStep;
            }
        }

        int getNumSteps() const noexcept { return numSteps; }
        SyncDiv getDivision() const noexcept { return division; }
        int getCurrentStepIndex() const noexcept { return lastStepIndex; }
        int getCurrentLoopLengthSamples() const noexcept { return loopSnippetLen; }
        int getNominalRepeatLengthSamples (StepAction action, double bpm) const noexcept
        {
            return repeatLengthSamples (action, bpm);
        }

    private:
        int repeatLengthSamples (StepAction action, double bpm) const noexcept
        {
            double beats = 1.0;
            switch (action)
            {
                case StepAction::Repeat4:  beats = 1.0; break;
                case StepAction::Repeat8:  beats = 0.5; break;
                case StepAction::Repeat16: beats = 0.25; break;
                case StepAction::Repeat32: beats = 0.125; break;
                default: break;
            }

            const double seconds = beats * 60.0 / juce::jmax (1.0, bpm);
            return juce::jlimit (2, juce::jmax (2, capacity - 4), (int) std::round (seconds * sampleRate));
        }

        void beginStep (StepAction action, int stepLengthSamples, double bpm) noexcept
        {
            switch (action)
            {
                case StepAction::Repeat4:
                case StepAction::Repeat8:
                case StepAction::Repeat16:
                case StepAction::Repeat32:
                    loopSnippetLen = repeatLengthSamples (action, bpm);
                    captureSnippet (loopSnippetLen, false);
                    loopCounter = 0;
                    break;
                case StepAction::Reverse:
                    loopSnippetLen = juce::jlimit (2, capacity - 4, stepLengthSamples);
                    captureSnippet (loopSnippetLen, true);
                    loopCounter = 0;
                    break;
                case StepAction::TapeStopDown:
                    playPos = (double) writePos;
                    rate = 1.0;
                    break;
                case StepAction::TapeStopUp:
                    playPos = (double) writePos - 8.0;
                    rate = 0.0;
                    break;
                case StepAction::PitchUp:
                    playPos = (double) writePos;
                    rate = 1.6;
                    loopSnippetLen = juce::jmax (4, stepLengthSamples);
                    break;
                case StepAction::PitchDown:
                    playPos = (double) writePos;
                    rate = 0.6;
                    break;
                case StepAction::Off:
                case StepAction::Gate:
                default:
                    break;
            }
            currentStepLengthSamples = stepLengthSamples;
        }

        void captureSnippet (int requestedLength, bool reversed) noexcept
        {
            const int available = juce::jmax (2, juce::jmin (validHistorySamples, capacity - 4));
            loopSnippetLen = juce::jlimit (2, available, requestedLength);
            const int mostRecent = (writePos - 1 + capacity) % capacity;

            for (int channel = 0; channel < 2; ++channel)
            {
                for (int sample = 0; sample < loopSnippetLen; ++sample)
                {
                    const int sourceIndex = reversed
                        ? (mostRecent - sample + capacity) % capacity
                        : (mostRecent - loopSnippetLen + 1 + sample + capacity) % capacity;
                    snippetBuf[(size_t) channel][(size_t) sample] = circBuf[(size_t) channel][(size_t) sourceIndex];
                }
            }
        }

        float readLoopSample (int channel, int counter) const noexcept
        {
            const int index = counter % loopSnippetLen;
            const int crossfadeLength = juce::jmin (loopSnippetLen / 4, juce::jmax (1, (int) (0.002 * sampleRate)));
            if (index >= crossfadeLength || crossfadeLength <= 1)
                return snippetBuf[(size_t) channel][(size_t) index];

            const int previousIndex = loopSnippetLen - crossfadeLength + index;
            const float alpha = (float) index / (float) crossfadeLength;
            return juce::jmap (alpha,
                snippetBuf[(size_t) channel][(size_t) previousIndex],
                snippetBuf[(size_t) channel][(size_t) index]);
        }

        float interpolatedCircularRead (int channel, double position) const noexcept
        {
            position = std::fmod (position, (double) capacity);
            while (position < 0.0)
                position += capacity;
            const int first = (int) position;
            const int second = (first + 1) % capacity;
            const float fraction = (float) (position - std::floor (position));
            const auto& data = circBuf[(size_t) channel];
            return data[(size_t) first] + fraction * (data[(size_t) second] - data[(size_t) first]);
        }

        void computeSample (float& processedLeft, float& processedRight) noexcept
        {
            switch (currentAction)
            {
                case StepAction::Repeat4:
                case StepAction::Repeat8:
                case StepAction::Repeat16:
                case StepAction::Repeat32:
                    processedLeft = readLoopSample (0, loopCounter);
                    processedRight = readLoopSample (1, loopCounter);
                    ++loopCounter;
                    break;
                case StepAction::Reverse:
                {
                    const int index = juce::jmin (loopCounter, loopSnippetLen - 1);
                    processedLeft = snippetBuf[0][(size_t) index];
                    processedRight = snippetBuf[1][(size_t) index];
                    ++loopCounter;
                    break;
                }
                case StepAction::TapeStopDown:
                {
                    const float progress = juce::jlimit (0.0f, 1.0f,
                        (float) sampleCounterInStep / (float) juce::jmax (1, currentStepLengthSamples));
                    rate = juce::jmax (0.0, 1.0 - (double) progress);
                    playPos += rate;
                    processedLeft = interpolatedCircularRead (0, playPos);
                    processedRight = interpolatedCircularRead (1, playPos);
                    break;
                }
                case StepAction::TapeStopUp:
                {
                    const float progress = juce::jlimit (0.0f, 1.0f,
                        (float) sampleCounterInStep / (float) juce::jmax (1, currentStepLengthSamples));
                    rate = (double) progress;
                    playPos += rate;
                    processedLeft = interpolatedCircularRead (0, playPos);
                    processedRight = interpolatedCircularRead (1, playPos);
                    break;
                }
                case StepAction::PitchUp:
                    playPos += rate;
                    if ((double) writePos - playPos < 32.0)
                        playPos -= loopSnippetLen;
                    processedLeft = interpolatedCircularRead (0, playPos);
                    processedRight = interpolatedCircularRead (1, playPos);
                    break;
                case StepAction::PitchDown:
                    playPos += rate;
                    processedLeft = interpolatedCircularRead (0, playPos);
                    processedRight = interpolatedCircularRead (1, playPos);
                    break;
                case StepAction::Gate:
                    processedLeft = 0.0f;
                    processedRight = 0.0f;
                    break;
                case StepAction::Off:
                default:
                    break;
            }
        }

        double sampleRate = 44100.0;
        int capacity = 352800;
        std::array<std::vector<float>, 2> circBuf, snippetBuf;
        int writePos = 0;
        int validHistorySamples = 0;

        bool enabled = false;
        int numSteps = 16;
        SyncDiv division = SyncDiv::d1_16;
        std::array<StepAction, maxSteps> pattern {};
        float mix = 1.0f;

        int lastStepIndex = -1;
        int sampleCounterInStep = 0;
        int currentStepLengthSamples = 4410;
        StepAction currentAction = StepAction::Off;

        double playPos = 0.0;
        double rate = 1.0;
        int loopSnippetLen = 1;
        int loopCounter = 0;
    };
}
'''
    current_stutter_engine = read("MotionFX_source/Source/DSP/StutterEngine.h")
    if "class StutterEngine" not in current_stutter_engine or "stepLenSamples / 32" not in current_stutter_engine:
        raise RuntimeError("StutterEngine.h is not the expected pre-Block-4 version")
    write("MotionFX_source/Source/DSP/StutterEngine.h", stutter_engine)

    # -------------------------------------------------------------------------
    # Stutter UI: clearer terminology, larger controls, and quick clear/alternate
    # actions. Repeats are explicitly documented as absolute musical lengths.
    # -------------------------------------------------------------------------
    stutter_panel = r'''#pragma once
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
'''
    current_stutter_panel = read("MotionFX_source/Source/GUI/StutterPanel.h")
    if "class StutterPanel" not in current_stutter_panel:
        raise RuntimeError("StutterPanel.h is not the expected MotionFX source file")
    write("MotionFX_source/Source/GUI/StutterPanel.h", stutter_panel)

    # -------------------------------------------------------------------------
    # Header sizing and About/Changelog accordion.
    # -------------------------------------------------------------------------
    replace_once(
        "MotionFX_source/Source/PluginEditor.h",
        """    void showAboutDialog();
    void showChangelogDialog();
    void showScrollableTextDialog (const juce::String& title, const juce::String& text);
""",
        """    void showAboutDialog (bool openChangelog = false);
    void showChangelogDialog();
""",
    )

    editor_path = "MotionFX_source/Source/PluginEditor.cpp"
    editor = read(editor_path)
    old_dialog_class = r'''    class ScrollableTextDialogContent final : public juce::Component
    {
    public:
        explicit ScrollableTextDialogContent (const juce::String& text)
        {
            editor.setMultiLine (true);
            editor.setReadOnly (true);
            editor.setScrollbarsShown (true);
            editor.setText (text, false);
            editor.setColour (juce::TextEditor::backgroundColourId, Palette::bg1);
            editor.setColour (juce::TextEditor::textColourId, Palette::text);
            editor.setColour (juce::TextEditor::outlineColourId, Palette::stroke);
            addAndMakeVisible (editor);
            setSize (620, 440);
        }

        void resized() override
        {
            editor.setBounds (getLocalBounds().reduced (12));
        }

    private:
        juce::TextEditor editor;
    };
'''
    new_dialog_class = r'''    class AboutDialogContent final : public juce::Component
    {
    public:
        AboutDialogContent (const juce::String& aboutText, const juce::String& changelogText, bool startExpanded)
        {
            configureEditor (aboutEditor, aboutText);
            configureEditor (changelogEditor, changelogText);
            addAndMakeVisible (aboutEditor);
            addAndMakeVisible (changelogToggle);
            addAndMakeVisible (changelogEditor);

            changelogToggle.onClick = [this]
            {
                setExpanded (! expanded);
            };

            setExpanded (startExpanded);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (12);
            aboutEditor.setBounds (bounds.removeFromTop (245));
            bounds.removeFromTop (8);
            changelogToggle.setBounds (bounds.removeFromTop (36));
            bounds.removeFromTop (8);
            if (expanded)
                changelogEditor.setBounds (bounds);
        }

    private:
        static void configureEditor (juce::TextEditor& editor, const juce::String& text)
        {
            editor.setMultiLine (true);
            editor.setReadOnly (true);
            editor.setScrollbarsShown (true);
            editor.setText (text, false);
            editor.setColour (juce::TextEditor::backgroundColourId, Palette::bg1);
            editor.setColour (juce::TextEditor::textColourId, Palette::text);
            editor.setColour (juce::TextEditor::outlineColourId, Palette::stroke);
        }

        void setExpanded (bool shouldExpand)
        {
            expanded = shouldExpand;
            changelogEditor.setVisible (expanded);
            changelogToggle.setButtonText (expanded ? "Hide changelog" : "Show changelog");
            setSize (660, expanded ? 600 : 330);
            resized();

            if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
                dialog->setContentComponentSize (getWidth(), getHeight());
        }

        juce::TextEditor aboutEditor, changelogEditor;
        juce::TextButton changelogToggle;
        bool expanded = false;
    };
'''
    if editor.count(old_dialog_class) != 1:
        raise RuntimeError("Could not find the Block 3 About dialog class in PluginEditor.cpp")
    editor = editor.replace(old_dialog_class, new_dialog_class, 1)

    old_header = r'''    auto header = b.removeFromTop (58);
    titleLabel.setBounds (header.removeFromLeft (170));

    auto masterArea = header.removeFromRight (330);
    matchGainToggle->setBounds (masterArea.removeFromRight (70).reduced (0, 14));
    dryWetKnob->setBounds (masterArea.removeFromRight (86));
    outputKnob->setBounds (masterArea.removeFromRight (86));
    inputKnob->setBounds (masterArea.removeFromRight (86));

    auto presetBar = header;
    optionsBtn.setBounds (presetBar.removeFromRight (36).reduced (2, 12));
    presetBar.removeFromRight (4);
    savePresetBtn.setBounds (presetBar.removeFromRight (56).reduced (0, 12));
    presetBar.removeFromRight (4);
    nextPresetBtn.setBounds (presetBar.removeFromRight (28).reduced (0, 12));
    prevPresetBtn.setBounds (presetBar.removeFromLeft (28).reduced (0, 12));
    presetNameButton.setBounds (presetBar.reduced (0, 12));

    b.removeFromTop (10);
    tabStrip.setBounds (b.removeFromTop (40));
'''
    new_header = r'''    auto header = b.removeFromTop (78);
    titleLabel.setBounds (header.removeFromLeft (170).reduced (0, 10));

    auto masterArea = header.removeFromRight (390);
    matchGainToggle->setBounds (masterArea.removeFromRight (88).reduced (2, 19));
    dryWetKnob->setBounds (masterArea.removeFromRight (98));
    outputKnob->setBounds (masterArea.removeFromRight (98));
    inputKnob->setBounds (masterArea.removeFromRight (98));

    auto presetBar = header;
    optionsBtn.setBounds (presetBar.removeFromRight (38).reduced (1, 20));
    presetBar.removeFromRight (5);
    savePresetBtn.setBounds (presetBar.removeFromRight (62).reduced (0, 20));
    presetBar.removeFromRight (5);
    nextPresetBtn.setBounds (presetBar.removeFromRight (32).reduced (0, 20));
    prevPresetBtn.setBounds (presetBar.removeFromLeft (32).reduced (0, 20));
    presetNameButton.setBounds (presetBar.reduced (0, 20));

    b.removeFromTop (8);
    tabStrip.setBounds (b.removeFromTop (42));
'''
    if editor.count(old_header) != 1:
        raise RuntimeError("Could not find the Block 3 header layout in PluginEditor.cpp")
    editor = editor.replace(old_header, new_header, 1)

    about_methods_pattern = r'''void MotionFXAudioProcessorEditor::showScrollableTextDialog \(const juce::String& title, const juce::String& text\)\n\{.*?\n\}\n\nvoid MotionFXAudioProcessorEditor::showAboutDialog\(\)\n\{.*?\n\}\n\nvoid MotionFXAudioProcessorEditor::showChangelogDialog\(\)\n\{.*?\n\}\n'''
    about_methods = r'''void MotionFXAudioProcessorEditor::showAboutDialog (bool openChangelog)
{
    const auto aboutText = juce::String (R"MFXABOUT(MotionFX 0.4.0 - Build 4

Multi-effect modulation VST3.

Direction and development: Paom
Some AI was used during the creation of this plugin, but all generated work was reviewed, reworked and proofed by humans.

Built with JUCE 8, C++20, CMake and the VST3 format.

Resources
- JUCE framework
- Steinberg VST3 SDK through JUCE
- GitHub Actions continuous integration

Click the MOTIONFX title at any time to reopen this window.)MFXABOUT");

    const auto changelogText = juce::String (R"MFXCHANGELOG(0.4.0 - Build 4
- Reworked the interface hierarchy and control sizing.
- Unified effect and modulation knob sizes.
- Added two-decimal numeric displays and direct value entry.
- Added Hz, seconds and tempo-synced timing for LFO, Motion and Sequencer modulators.
- Added dotted and additional triplet timing divisions while preserving old preset indices.
- Enlarged mode selectors and reorganised related modulation controls.
- Corrected Stutter repeat lengths so 1/4, 1/8, 1/16 and 1/32 are absolute musical values.
- Added Stutter loop-boundary crossfades, Clear and alternating 1/8 helpers.
- Consolidated About and Changelog into one expandable window.
- Registered the audio and GUI executables with CTest.

0.3.0 - Build 3
- Preset identity and modified-state persistence.
- Clean Init state.
- Header, preset and modulation-source readability fixes.
- Stable drag-and-drop tab identity.
- Direct numeric value entry and compact decimals.
- About, resources and changelog windows.

0.2.0 - Build 2
- Preset browser and recursive user folders.
- Portable CMake and automated Windows/Linux builds.

0.1.0 - Build 1
- DSP pause on stopped host transport.
- Selected-effect identity preserved during reorder.
- Gain Match naming update.)MFXCHANGELOG");

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (new AboutDialogContent (aboutText, changelogText, openChangelog));
    options.dialogTitle = "About MotionFX";
    options.dialogBackgroundColour = Palette::bg0;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}

void MotionFXAudioProcessorEditor::showChangelogDialog()
{
    showAboutDialog (true);
}
'''
    editor_new, count = re.subn(about_methods_pattern, about_methods, editor, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError("Could not replace About/Changelog methods in PluginEditor.cpp")
    editor = editor_new
    editor = editor.replace('menu.addItem (7, "About MotionFX...");\n    menu.addItem (8, "Changelog...");',
                            'menu.addItem (7, "About / Changelog...");\n    menu.addItem (8, "Open Changelog...");')
    write(editor_path, editor)

    # -------------------------------------------------------------------------
    # Automated checks for the new timing choices and absolute Stutter lengths.
    # -------------------------------------------------------------------------
    test_path = "MotionFX_source/Source/Tests/AudioIntegrityTest.cpp"
    test = read(test_path)
    insertion_marker = '    if (numFactory != 16) { std::cout << "  [FAIL] preset count mismatch" << std::endl; ++failures; }\n'
    if test.count(insertion_marker) != 1:
        raise RuntimeError("Could not find AudioIntegrityTest insertion point")
    test_block = r'''

    // Block 4 timing choices and absolute Stutter repeat lengths.
    if (mfx::syncDivChoices().size() != mfx::syncDivCount)
    {
        std::cout << "  [FAIL] sync division UI/DSP count mismatch" << std::endl;
        ++failures;
    }

    const auto nearlyEqual = [] (double a, double b) { return std::abs (a - b) < 1.0e-6; };
    if (! nearlyEqual (mfx::syncDivToBeats (mfx::SyncDiv::d1_4D), 1.5)
        || ! nearlyEqual (mfx::syncDivToBeats (mfx::SyncDiv::d1_4T), 2.0 / 3.0)
        || ! nearlyEqual (mfx::syncDivToBeats (mfx::SyncDiv::d1_16D), 0.375))
    {
        std::cout << "  [FAIL] dotted/triplet sync conversion mismatch" << std::endl;
        ++failures;
    }

    for (auto* id : { "drive", "pan", "volume", "space", "retro", "width" })
    {
        const juce::String prefix (id);
        for (auto* suffix : { "lfo_rateunit", "motion_rateunit", "seq_rateunit" })
        {
            if (proc.apvts.getParameter (prefix + "_" + suffix) == nullptr)
            {
                std::cout << "  [FAIL] missing timing unit parameter " << prefix << "_" << suffix << std::endl;
                ++failures;
            }
        }
    }

    mfx::StutterEngine stutterTimingTest;
    stutterTimingTest.prepare (48000.0);
    if (stutterTimingTest.getNominalRepeatLengthSamples (mfx::StepAction::Repeat4, 120.0) != 24000
        || stutterTimingTest.getNominalRepeatLengthSamples (mfx::StepAction::Repeat8, 120.0) != 12000
        || stutterTimingTest.getNominalRepeatLengthSamples (mfx::StepAction::Repeat16, 120.0) != 6000
        || stutterTimingTest.getNominalRepeatLengthSamples (mfx::StepAction::Repeat32, 120.0) != 3000)
    {
        std::cout << "  [FAIL] Stutter repeat lengths are not absolute musical divisions" << std::endl;
        ++failures;
    }
'''
    test = test.replace(insertion_marker, insertion_marker + test_block, 1)
    write(test_path, test)

    # -------------------------------------------------------------------------
    # Human-readable changelog.
    # -------------------------------------------------------------------------
    changelog = """# MotionFX changelog

## 0.4.0 - Block 4

- Reworked the interface hierarchy so related modulation controls are grouped together.
- Fixed rotary controls becoming tiny inside wide layout cells.
- Enlarged and unified master, effect and modulation knob sizes.
- Enlarged effect-mode selectors and their popup text.
- Limited float controls to two decimal places and retained double-click numeric entry.
- Added Hz or seconds units for free-running LFO, Motion and Sequencer rates.
- Added dotted and additional triplet tempo divisions while preserving existing preset indices.
- Made tempo-synced and free-running controls switch contextually instead of displaying conflicting values.
- Reworked Stutter repeat timing so Repeat 1/4, 1/8, 1/16 and 1/32 are absolute musical lengths.
- Added short Stutter loop-boundary crossfades plus Clear and alternating 1/8 helpers.
- Replaced ambiguous About credits with the Paom credit and human-review AI disclosure.
- Consolidated About and Changelog into one expandable window using ASCII-safe text.
- Updated the project/VST3 version to `0.4.0`.
- Registered the audio and GUI test executables with CTest so CI runs real tests.

## 0.3.0 - Block 3

- Matched the preset-name control height to the other header buttons.
- Removed the redundant hamburger preset-menu button.
- Fixed the dragged effect tab changing its displayed identity while crossing another slot.
- Enlarged modulation-source selectors and their popup text for readability.
- Made Init start with every effect module and hidden sync toggle disabled.
- Persisted the loaded preset identity in DAW project/session state.
- Added an asterisk when the current parameters differ from the loaded or saved preset.
- Added double-click direct value entry on knobs.
- Added About, changelog and preset-folder shortcuts.
- Updated GitHub Actions to Node.js 24-compatible action versions.

## 0.2.0 - Block 2

- Added the preset browser and recursive user preset folders.
- Added portable CMake/JUCE acquisition and automated Windows/Linux builds.

## 0.1.0 - Block 1

- Paused DSP when the host transport is stopped.
- Preserved selected-effect identity during effect reordering.
- Renamed the master toggle to `GAIN MATCH`.
"""
    write("MotionFX_source/CHANGELOG.md", changelog)

except Exception as exc:
    print(f"ERROR: {exc}")
    sys.exit(1)

print("MotionFX Block 4 applied successfully.")
print(f"Modified {len(changed)} files:")
for path in changed:
    print(f"  - {path}")
print("\nNext: git diff --check, commit, push, then inspect both GitHub Actions jobs.")
