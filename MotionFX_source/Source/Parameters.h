#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/Modulation.h"
#include "DSP/EffectChain.h"

namespace mfx
{
    inline juce::StringArray syncDivChoices()
    {
        return { "4 Bar", "2 Bar", "1 Bar",
                 "1/2", "1/2T", "1/2D",
                 "1/4", "1/4T", "1/4D",
                 "1/8", "1/8T", "1/8D",
                 "1/16", "1/16T", "1/16D",
                 "1/32", "1/32T", "1/32D" };
    }

    inline juce::StringArray rateUnitChoices() { return { "Hz", "Seconds" }; }
    inline juce::StringArray modSourceChoices() { return { "Off", "LFO", "Env Follower", "Motion", "Sequencer" }; }
    inline juce::StringArray lfoShapeChoices() { return { "Sine", "Triangle", "Square", "Saw" }; }
    inline juce::StringArray motionModeChoices() { return { "Loop", "One-Shot", "Trigger" }; }
    inline juce::StringArray motionShapeChoices() { return { "Pluck Then Rise", "Rise Then Fall", "Ramp", "Dip", "Flat" }; }

    static constexpr const char* effectPrefixes[numEffects] = {
        "drive", "pan", "volume", "space", "retro", "width", "filter"
    };

    static constexpr const char* effectDisplayNames[numEffects] = {
        "Drive", "Pan", "Volume", "Space", "Retro", "Width", "Filter"
    };

    inline juce::StringArray driveModeChoices() { return { "Overdrive", "Tube", "Soft Clip", "Hard Clip", "Tape", "Wavefold", "Sinoid Fold", "Groove Phase" }; }
    inline juce::StringArray driveQualityChoices() { return { "Eco", "2x", "4x" }; }
    inline juce::StringArray drivePostClipChoices() { return { "None", "Soft", "Hard", "True Peak" }; }
    inline juce::StringArray grooveCharacterChoices() { return { "Soft", "Hard" }; }
    inline juce::StringArray panModeChoices() { return { "Linear", "Ping-Pong", "Rotary" }; }
    inline juce::StringArray volumeModeChoices() { return { "Linear", "Exponential", "Gate", "Duck" }; }
    inline juce::StringArray spaceModeChoices() { return { "Plate", "Hall", "Echo Delay", "Pan Delay", "Gated Reverb", "Tape Delay", "Shimmer" }; }
    inline juce::StringArray retroModeChoices() { return { "Bitcrush", "Lossy", "Wear & Tear", "E-mu 12-Bit" }; }
    inline juce::StringArray widthModeChoices() { return { "Stereo Width", "Haas", "Mono Bass", "Phase" }; }
    inline juce::StringArray filterModeChoices() { return { "Low Pass", "High Pass", "Band Pass", "Notch", "Peak", "Comb" }; }
    inline juce::StringArray filterSlopeChoices() { return { "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }; }

    inline juce::StringArray stutterRepeatChoices()
    {
        return { "Off", "1/4", "1/8", "1/16", "1/32" };
    }

    inline juce::StringArray stutterTapeChoices()
    {
        return { "Off", "Down", "Up" };
    }

    inline juce::String pidFor (const juce::String& prefix, const juce::String& name)
    {
        return prefix + "_" + name;
    }

    using APF = juce::AudioProcessorValueTreeState;
    using Layout = std::vector<std::unique_ptr<juce::RangedAudioParameter>>;

    inline void addModBlock (Layout& parameters, const juce::String& prefix, int versionHint,
                             float baseMaximum = 100.0f,
                             float baseDefault = 50.0f)
    {
        juce::ignoreUnused (versionHint);

        parameters.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID (pidFor (prefix, "enabled"), 1), prefix + " Enabled", false));
        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "base"), 1), prefix + " Amount",
            juce::NormalisableRange<float> (0.0f, baseMaximum), baseDefault));
        parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "modsource"), 1), prefix + " Mod Source", modSourceChoices(), 0));
        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "moddepth"), 1), prefix + " Mod Depth",
            juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f));

        parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "lfo_shape"), 1), prefix + " LFO Shape", lfoShapeChoices(), 0));
        parameters.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID (pidFor (prefix, "lfo_synced"), 1), prefix + " LFO Synced", false));
        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "lfo_rate"), 1), prefix + " LFO Rate",
            juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.35f), 1.0f));
        parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "lfo_rateunit"), 1), prefix + " LFO Rate Unit", rateUnitChoices(), 0));
        parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "lfo_div"), 1), prefix + " LFO Division", syncDivChoices(), 6));

        parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "motion_mode"), 1), prefix + " Motion Mode", motionModeChoices(), 0));
        parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "motion_shape"), 1), prefix + " Motion Shape", motionShapeChoices(), 0));
        parameters.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID (pidFor (prefix, "motion_synced"), 1), prefix + " Motion Synced", false));
        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "motion_rate"), 1), prefix + " Motion Rate",
            juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.35f), 0.5f));
        parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "motion_rateunit"), 1), prefix + " Motion Rate Unit", rateUnitChoices(), 0));
        parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "motion_div"), 1), prefix + " Motion Division", syncDivChoices(), 2));
        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "motion_smooth"), 1), prefix + " Motion Smooth",
            juce::NormalisableRange<float> (0.0f, 100.0f), 20.0f));

        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "env_attack"), 1), prefix + " Env Attack",
            juce::NormalisableRange<float> (0.1f, 500.0f, 0.0f, 0.4f), 10.0f));
        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "env_release"), 1), prefix + " Env Release",
            juce::NormalisableRange<float> (1.0f, 2000.0f, 0.0f, 0.4f), 150.0f));

        parameters.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (pidFor (prefix, "seq_numsteps"), 1), prefix + " Seq Steps", 1, 32, 8));
        parameters.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID (pidFor (prefix, "seq_synced"), 1), prefix + " Seq Synced", false));
        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "seq_rate"), 1), prefix + " Seq Rate",
            juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.35f), 2.0f));
        parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "seq_rateunit"), 1), prefix + " Seq Rate Unit", rateUnitChoices(), 0));
        parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "seq_div"), 1), prefix + " Seq Division", syncDivChoices(), 9));
        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "seq_smooth"), 1), prefix + " Seq Smooth",
            juce::NormalisableRange<float> (0.0f, 50.0f), 2.0f));

        for (int index = 0; index < StepSequencer::maxSteps; ++index)
            parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID (pidFor (prefix, "seq_step" + juce::String (index)), 1),
                prefix + " Seq Step " + juce::String (index + 1),
                juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
    }

    inline APF::ParameterLayout createParameterLayout()
    {
        Layout parameters;

        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID ("master_input", 1), "Input Gain",
            juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f));
        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID ("master_output", 1), "Output Gain",
            juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f));
        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID ("master_drywet", 1), "Dry/Wet",
            juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
        parameters.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID ("master_matchgain", 1), "Match Gain", false));

        for (int effect = 0; effect < numEffects; ++effect)
        {
            const juce::String prefix = effectPrefixes[effect];
            addModBlock (parameters, prefix, 1,
                         prefix == "width" ? 200.0f : 100.0f,
                         prefix == "width" ? 100.0f : 50.0f);

            if (prefix == "drive")
            {
                parameters.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("drive_mode", 1), "Drive Mode", driveModeChoices(), 1));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("drive_tone", 1), "Drive Tone", juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("drive_bias", 1), "Drive Bias", juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("drive_mix", 1), "Drive Mix", juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("drive_outtrim", 1), "Drive Out Trim", juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("drive_quality", 1), "Drive Oversampling", driveQualityChoices(), 1));
                parameters.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("drive_postclip", 1), "Drive Post Clip", drivePostClipChoices(), 0));

                parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID ("drive_trace_enabled", 1), "Tracing Model Enabled", true));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("drive_trace_gain", 1), "Tracing Model Gain", juce::NormalisableRange<float> (0.0f, 24.0f), 6.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("drive_trace_freq", 1), "Tracing Model Frequency", juce::NormalisableRange<float> (40.0f, 18000.0f, 0.0f, 0.35f), 684.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("drive_trace_bandwidth", 1), "Tracing Model Bandwidth", juce::NormalisableRange<float> (0.20f, 4.0f), 0.32f));

                parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID ("drive_pinch_enabled", 1), "Pinch Enabled", true));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("drive_pinch_gain", 1), "Pinch Gain", juce::NormalisableRange<float> (0.0f, 24.0f), 6.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("drive_pinch_freq", 1), "Pinch Frequency", juce::NormalisableRange<float> (40.0f, 18000.0f, 0.0f, 0.35f), 7500.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("drive_pinch_bandwidth", 1), "Pinch Bandwidth", juce::NormalisableRange<float> (0.20f, 4.0f), 3.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("drive_groove_character", 1), "Groove Character", grooveCharacterChoices(), 0));
                parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID ("drive_pinch_stereo", 1), "Pinch Stereo", true));
            }
            else if (prefix == "pan")
            {
                parameters.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("pan_mode", 1), "Pan Mode", panModeChoices(), 0));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("pan_widthinfluence", 1), "Pan Width Influence", juce::NormalisableRange<float> (0.0f, 100.0f), 30.0f));
            }
            else if (prefix == "volume")
            {
                parameters.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("volume_mode", 1), "Volume Mode", volumeModeChoices(), 0));
            }
            else if (prefix == "space")
            {
                parameters.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("space_mode", 1), "Space Mode", spaceModeChoices(), 0));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("space_size", 1), "Space Size or Delay Time", juce::NormalisableRange<float> (0.0f, 100.0f), 40.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("space_decay", 1), "Space Decay or Feedback", juce::NormalisableRange<float> (0.0f, 100.0f), 40.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("space_tone", 1), "Space Tone", juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID ("space_delay_synced", 1), "Delay Tempo Sync", false));
                parameters.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("space_delay_rateunit", 1), "Delay Time Unit", rateUnitChoices(), 1));
                parameters.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("space_delay_div", 1), "Delay Division", syncDivChoices(), 6));
            }
            else if (prefix == "retro")
            {
                parameters.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("retro_mode", 1), "Retro Mode", retroModeChoices(), 0));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("retro_rate", 1), "Retro Rate", juce::NormalisableRange<float> (0.0f, 100.0f), 30.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("retro_tone", 1), "Retro Tone", juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("retro_mix", 1), "Retro Mix", juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
            }
            else if (prefix == "width")
            {
                parameters.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("width_mode", 1), "Width Mode", widthModeChoices(), 0));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("width_crossover", 1), "Width Crossover", juce::NormalisableRange<float> (0.0f, 100.0f), 40.0f));
            }
            else if (prefix == "filter")
            {
                parameters.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("filter_mode", 1), "Filter Mode", filterModeChoices(), 0));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("filter_resonance", 1), "Filter Resonance", juce::NormalisableRange<float> (0.0f, 100.0f), 20.0f));
                parameters.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("filter_slope", 1), "Filter Slope", filterSlopeChoices(), 1));
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("filter_mix", 1), "Filter Mix", juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
            }
        }

        parameters.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID ("stutter_enabled", 1), "Stutter Enabled", false));
        parameters.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID ("stutter_numsteps", 1), "Stutter Steps", 1, 32, 16));
        parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID ("stutter_div", 1), "Stutter Division", syncDivChoices(), 12));
        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID ("stutter_mix", 1), "Stutter Mix",
            juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID ("stutter_pitch_grain_ms", 1), "Stutter Pitch Grain",
            juce::NormalisableRange<float> (10.0f, 120.0f, 0.1f), 50.0f));

        for (int index = 0; index < StutterEngine::maxSteps; ++index)
        {
            const juce::String step (index);
            parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID ("stutter_repeat_step" + step, 1),
                "Stutter Repeat Step " + juce::String (index + 1),
                stutterRepeatChoices(), 0));
            parameters.push_back (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID ("stutter_reverse_step" + step, 1),
                "Stutter Reverse Step " + juce::String (index + 1), false));
            parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID ("stutter_tape_step" + step, 1),
                "Stutter Tape Step " + juce::String (index + 1),
                stutterTapeChoices(), 0));
            parameters.push_back (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID ("stutter_pitch_step" + step, 1),
                "Stutter Pitch Step " + juce::String (index + 1), false));
            parameters.push_back (std::make_unique<juce::AudioParameterInt> (
                juce::ParameterID ("stutter_pitch_semitones_step" + step, 1),
                "Stutter Pitch Semitones Step " + juce::String (index + 1),
                -24, 24, 0));
            parameters.push_back (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID ("stutter_gate_step" + step, 1),
                "Stutter Gate Step " + juce::String (index + 1), false));
        }

        return { parameters.begin(), parameters.end() };
    }
}
