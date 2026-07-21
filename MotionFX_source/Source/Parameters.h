#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/Modulation.h"
#include "DSP/EffectChain.h"

namespace mfx
{
    inline juce::StringArray syncDivChoices()
    {
        return { "4 Bar", "2 Bar", "1 Bar", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    }

    inline juce::StringArray modSourceChoices() { return { "Off", "LFO", "Env Follower", "Motion", "Sequencer" }; }
    inline juce::StringArray lfoShapeChoices()  { return { "Sine", "Triangle", "Square", "Saw" }; }
    inline juce::StringArray motionModeChoices() { return { "Loop", "One-Shot", "Trigger" }; }
    inline juce::StringArray motionShapeChoices() { return { "Pluck Then Rise", "Rise Then Fall", "Ramp", "Dip", "Flat" }; }

    static constexpr const char* effectPrefixes[numEffects] = { "drive", "pan", "volume", "space", "retro", "width" };
    static constexpr const char* effectDisplayNames[numEffects] = { "Drive", "Pan", "Volume", "Space", "Retro", "Width" };

    inline juce::StringArray driveModeChoices()  { return { "Overdrive", "Tube", "Soft Clip", "Hard Clip", "Vinyl", "Phase Distort" }; }
    inline juce::StringArray panModeChoices()    { return { "Linear", "Ping-Pong", "Rotary" }; }
    inline juce::StringArray volumeModeChoices() { return { "Linear", "Exponential", "Gate", "Duck" }; }
    inline juce::StringArray spaceModeChoices()  { return { "Plate", "Hall", "Echo Delay", "Pan Delay", "Gated Reverb", "Tape Delay", "Shimmer" }; }
    inline juce::StringArray retroModeChoices()  { return { "Bitcrush", "Lossy", "Wear & Tear", "E-mu 12-Bit" }; }
    inline juce::StringArray widthModeChoices()  { return { "Stereo Width", "Haas", "Mono Bass", "Phase" }; }
    inline juce::StringArray stutterActionChoices()
    {
        return { "Off", "Repeat 1/4", "Repeat 1/8", "Repeat 1/16", "Repeat 1/32",
                 "Reverse", "Tape Stop", "Tape Up", "Pitch Up", "Pitch Down", "Gate" };
    }

    inline juce::String pidFor (const juce::String& prefix, const juce::String& name) { return prefix + "_" + name; }

    using APF = juce::AudioProcessorValueTreeState;
    using Layout = std::vector<std::unique_ptr<juce::RangedAudioParameter>>;

    inline void addModBlock (Layout& p, const juce::String& prefix, int versionHint)
    {
        juce::ignoreUnused (versionHint);
        p.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID (pidFor (prefix, "enabled"), 1), prefix + " Enabled", true));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "base"), 1), prefix + " Amount",
            juce::NormalisableRange<float> (0.0f, 100.0f), 50.0f));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "modsource"), 1), prefix + " Mod Source", modSourceChoices(), 0));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "moddepth"), 1), prefix + " Mod Depth",
            juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f));

        // LFO sub-block
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "lfo_shape"), 1), prefix + " LFO Shape", lfoShapeChoices(), 0));
        p.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID (pidFor (prefix, "lfo_synced"), 1), prefix + " LFO Synced", true));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "lfo_rate"), 1), prefix + " LFO Rate Hz",
            juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.35f), 1.0f));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "lfo_div"), 1), prefix + " LFO Division", syncDivChoices(), 4));

        // Motion envelope sub-block
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "motion_mode"), 1), prefix + " Motion Mode", motionModeChoices(), 0));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "motion_shape"), 1), prefix + " Motion Shape", motionShapeChoices(), 0));
        p.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID (pidFor (prefix, "motion_synced"), 1), prefix + " Motion Synced", true));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "motion_rate"), 1), prefix + " Motion Rate Hz",
            juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.35f), 0.5f));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "motion_div"), 1), prefix + " Motion Division", syncDivChoices(), 2));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "motion_smooth"), 1), prefix + " Motion Smooth",
            juce::NormalisableRange<float> (0.0f, 100.0f), 20.0f));

        // Envelope follower sub-block
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "env_attack"), 1), prefix + " Env Attack",
            juce::NormalisableRange<float> (0.1f, 500.0f, 0.0f, 0.4f), 10.0f));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "env_release"), 1), prefix + " Env Release",
            juce::NormalisableRange<float> (1.0f, 2000.0f, 0.0f, 0.4f), 150.0f));

        // Step sequencer sub-block
        p.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (pidFor (prefix, "seq_numsteps"), 1), prefix + " Seq Steps", 1, 32, 8));
        p.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID (pidFor (prefix, "seq_synced"), 1), prefix + " Seq Synced", true));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "seq_rate"), 1), prefix + " Seq Rate Hz",
            juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.35f), 2.0f));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pidFor (prefix, "seq_div"), 1), prefix + " Seq Division", syncDivChoices(), 5));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pidFor (prefix, "seq_smooth"), 1), prefix + " Seq Smooth",
            juce::NormalisableRange<float> (0.0f, 50.0f), 2.0f));
        for (int i = 0; i < StepSequencer::maxSteps; ++i)
            p.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID (pidFor (prefix, "seq_step" + juce::String (i)), 1),
                prefix + " Seq Step " + juce::String (i + 1),
                juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
    }

    inline APF::ParameterLayout createParameterLayout()
    {
        Layout p;

        // Master
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID ("master_input", 1), "Input Gain",
            juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID ("master_output", 1), "Output Gain",
            juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID ("master_drywet", 1), "Dry/Wet",
            juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
        p.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID ("master_matchgain", 1), "Match Gain", false));

        // 6 effects
        for (int e = 0; e < numEffects; ++e)
        {
            juce::String prefix = effectPrefixes[e];
            addModBlock (p, prefix, 1);

            if (prefix == "drive")
            {
                p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("drive_mode", 1), "Drive Mode", driveModeChoices(), 1));
                p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("drive_tone", 1), "Drive Tone", juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f));
                p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("drive_mix", 1), "Drive Mix", juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
                p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("drive_outtrim", 1), "Drive Out Trim", juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f));
            }
            else if (prefix == "pan")
            {
                p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("pan_mode", 1), "Pan Mode", panModeChoices(), 0));
                p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("pan_widthinfluence", 1), "Pan Width Influence", juce::NormalisableRange<float> (0.0f, 100.0f), 30.0f));
            }
            else if (prefix == "volume")
            {
                p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("volume_mode", 1), "Volume Mode", volumeModeChoices(), 0));
            }
            else if (prefix == "space")
            {
                p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("space_mode", 1), "Space Mode", spaceModeChoices(), 0));
                p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("space_size", 1), "Space Size", juce::NormalisableRange<float> (0.0f, 100.0f), 40.0f));
                p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("space_decay", 1), "Space Decay", juce::NormalisableRange<float> (0.0f, 100.0f), 40.0f));
                p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("space_tone", 1), "Space Tone", juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f));
            }
            else if (prefix == "retro")
            {
                p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("retro_mode", 1), "Retro Mode", retroModeChoices(), 0));
                p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("retro_rate", 1), "Retro Rate", juce::NormalisableRange<float> (0.0f, 100.0f), 30.0f));
                p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("retro_tone", 1), "Retro Tone", juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f));
                p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("retro_mix", 1), "Retro Mix", juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
            }
            else if (prefix == "width")
            {
                p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("width_mode", 1), "Width Mode", widthModeChoices(), 0));
                p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("width_crossover", 1), "Width Crossover", juce::NormalisableRange<float> (0.0f, 100.0f), 40.0f));
            }
        }

        // Stutter / repeat / tape-stop tab
        p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID ("stutter_enabled", 1), "Stutter Enabled", false));
        p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID ("stutter_numsteps", 1), "Stutter Steps", 1, 32, 16));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID ("stutter_div", 1), "Stutter Division", syncDivChoices(), 7));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("stutter_mix", 1), "Stutter Mix", juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
        for (int i = 0; i < StutterEngine::maxSteps; ++i)
            p.push_back (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID ("stutter_step" + juce::String (i), 1), "Stutter Step " + juce::String (i + 1),
                stutterActionChoices(), 0));

        return { p.begin(), p.end() };
    }
}
