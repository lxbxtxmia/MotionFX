#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace mfx
{
    struct FactoryPreset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> overrides; // paramID -> real-world value (index for choice/bool)
    };

    // choice index shorthands, mirroring the StringArrays in Parameters.h
    namespace idx
    {
        enum ModSrc { Off = 0, ModLfo = 1, Env = 2, Motion = 3, Seq = 4 };
        enum Div {
            Bar4 = 0, Bar2 = 1, Bar1 = 2,
            D1_2 = 3, D1_2T = 4, D1_2D = 5,
            D1_4 = 6, D1_4T = 7, D1_4D = 8,
            D1_8 = 9, D1_8T = 10, D1_8D = 11,
            D1_16 = 12, D1_16T = 13, D1_16D = 14,
            D1_32 = 15, D1_32T = 16, D1_32D = 17
        };
        enum LfoShape { Sine = 0, Tri = 1, Sqr = 2, Saw = 3 };
        enum MotionShape { Pluck = 0, RiseFall = 1, Ramp = 2, Dip = 3, Flat = 4 };
        enum DriveMode { Overdrive = 0, Tube = 1, SoftClip = 2, HardClip = 3, Tape = 4, Wavefold = 5, SinoidFold = 6, GroovePhase = 7 };
        enum PanMode { PanLinear = 0, PingPong = 1, Rotary = 2 };
        enum VolMode { VolLinear = 0, Exponential = 1, Gate = 2, Duck = 3 };
        enum SpaceMode { Plate = 0, Hall = 1, EchoDelay = 2, PanDelay = 3, GatedReverb = 4, TapeDelay = 5, Shimmer = 6 };
        enum RetroMode { Bitcrush = 0, Lossy = 1, WearTear = 2, Emu12 = 3 };
        enum WidthMode { StereoWidth = 0, Haas = 1, MonoBass = 2, Phase = 3 };
        enum StutterAct { SOff = 0, Rep4 = 1, Rep8 = 2, Rep16 = 3, Rep32 = 4, Rev = 5, TStop = 6, TUp = 7, PUp = 8, PDown = 9, SGate = 10 };
    }

    inline void setStutterPattern (FactoryPreset& fp, int numSteps, int div, const std::vector<int>& actions)
    {
        fp.overrides.push_back ({ "stutter_enabled", 1.0f });
        fp.overrides.push_back ({ "stutter_numsteps", (float) numSteps });
        fp.overrides.push_back ({ "stutter_div", (float) div });
        for (int i = 0; i < (int) actions.size() && i < 32; ++i)
            fp.overrides.push_back ({ "stutter_step" + juce::String (i), (float) actions[(size_t) i] });
    }

    inline std::vector<FactoryPreset> getFactoryPresets()
    {
        using namespace idx;
        std::vector<FactoryPreset> out;

        // ---------- Sample chopping ----------
        {
            FactoryPreset fp; fp.name = "Chop Shop";
            setStutterPattern (fp, 8, D1_8, { Rep8, SOff, Rev, SOff, Rep16, Rep16, SOff, TStop });
            fp.overrides.push_back ({ "stutter_mix", 100.0f });
            fp.overrides.push_back ({ "retro_enabled", 1.0f });
            fp.overrides.push_back ({ "retro_mode", (float) Bitcrush });
            fp.overrides.push_back ({ "retro_base", 25.0f });
            fp.overrides.push_back ({ "retro_mix", 60.0f });
            out.push_back (fp);
        }
        {
            FactoryPreset fp; fp.name = "Glitch Slice";
            setStutterPattern (fp, 16, D1_16, { Rep16, SOff, SGate, Rep32, Rev, SOff, Rep8, SOff,
                                                 SGate, Rep16, SOff, PUp, SOff, Rev, Rep32, SOff });
            fp.overrides.push_back ({ "stutter_mix", 100.0f });
            fp.overrides.push_back ({ "width_enabled", 1.0f });
            fp.overrides.push_back ({ "width_mode", (float) StereoWidth });
            fp.overrides.push_back ({ "width_base", 65.0f });
            out.push_back (fp);
        }
        {
            FactoryPreset fp; fp.name = "Tape Chopper";
            setStutterPattern (fp, 8, D1_4, { SOff, TStop, SOff, Rep8, TUp, SOff, Rev, TStop });
            fp.overrides.push_back ({ "stutter_mix", 90.0f });
            fp.overrides.push_back ({ "retro_enabled", 1.0f });
            fp.overrides.push_back ({ "retro_mode", (float) WearTear });
            fp.overrides.push_back ({ "retro_base", 35.0f });
            fp.overrides.push_back ({ "retro_rate", 40.0f });
            fp.overrides.push_back ({ "retro_mix", 70.0f });
            out.push_back (fp);
        }
        {
            FactoryPreset fp; fp.name = "Micro Cuts";
            setStutterPattern (fp, 32, D1_32, { SGate, SOff, SGate, SOff, Rep32, SOff, SGate, SOff,
                                                 SGate, SOff, Rep32, SOff, SGate, SOff, SGate, SOff,
                                                 SGate, SOff, SGate, SOff, Rep32, SOff, SGate, SOff,
                                                 SGate, SOff, Rep32, SOff, SGate, SOff, SGate, SOff });
            fp.overrides.push_back ({ "stutter_mix", 100.0f });
            out.push_back (fp);
        }

        // ---------- Cool trance rhythm ----------
        {
            FactoryPreset fp; fp.name = "Trance Gate";
            fp.overrides.push_back ({ "volume_enabled", 1.0f });
            fp.overrides.push_back ({ "volume_mode", (float) Gate });
            fp.overrides.push_back ({ "volume_base", 50.0f });
            fp.overrides.push_back ({ "volume_modsource", (float) Seq });
            fp.overrides.push_back ({ "volume_moddepth", 100.0f });
            fp.overrides.push_back ({ "volume_seq_numsteps", 16.0f });
            fp.overrides.push_back ({ "volume_seq_synced", 1.0f });
            fp.overrides.push_back ({ "volume_seq_div", (float) D1_16 });
            fp.overrides.push_back ({ "volume_seq_smooth", 1.0f });
            const int pattern[16] = { 100,0,100,0, 100,100,0,100, 0,100,0,100, 100,0,0,100 };
            for (int i = 0; i < 16; ++i)
                fp.overrides.push_back ({ "volume_seq_step" + juce::String (i), (float) pattern[i] });
            out.push_back (fp);
        }
        {
            FactoryPreset fp; fp.name = "Supersaw Pump";
            fp.overrides.push_back ({ "volume_enabled", 1.0f });
            fp.overrides.push_back ({ "volume_mode", (float) Duck });
            fp.overrides.push_back ({ "volume_base", 60.0f });
            fp.overrides.push_back ({ "volume_modsource", (float) Env });
            fp.overrides.push_back ({ "volume_moddepth", 80.0f });
            fp.overrides.push_back ({ "volume_env_attack", 3.0f });
            fp.overrides.push_back ({ "volume_env_release", 220.0f });
            fp.overrides.push_back ({ "pan_enabled", 1.0f });
            fp.overrides.push_back ({ "pan_mode", (float) PingPong });
            fp.overrides.push_back ({ "pan_modsource", (float) ModLfo });
            fp.overrides.push_back ({ "pan_moddepth", 70.0f });
            fp.overrides.push_back ({ "pan_lfo_synced", 1.0f });
            fp.overrides.push_back ({ "pan_lfo_div", (float) D1_8 });
            out.push_back (fp);
        }
        {
            FactoryPreset fp; fp.name = "Uplifter Delay";
            fp.overrides.push_back ({ "space_enabled", 1.0f });
            fp.overrides.push_back ({ "space_mode", (float) PanDelay });
            fp.overrides.push_back ({ "space_base", 35.0f });
            fp.overrides.push_back ({ "space_size", 30.0f });
            fp.overrides.push_back ({ "space_decay", 55.0f });
            fp.overrides.push_back ({ "space_modsource", (float) Seq });
            fp.overrides.push_back ({ "space_moddepth", 90.0f });
            fp.overrides.push_back ({ "space_seq_numsteps", 8.0f });
            fp.overrides.push_back ({ "space_seq_div", (float) D1_4 });
            const int pattern[8] = { 10, 80, 10, 60, 10, 100, 10, 40 };
            for (int i = 0; i < 8; ++i)
                fp.overrides.push_back ({ "space_seq_step" + juce::String (i), (float) pattern[i] });
            fp.overrides.push_back ({ "width_enabled", 1.0f });
            fp.overrides.push_back ({ "width_mode", (float) Haas });
            fp.overrides.push_back ({ "width_modsource", (float) Motion });
            fp.overrides.push_back ({ "width_moddepth", 50.0f });
            fp.overrides.push_back ({ "width_motion_shape", (float) RiseFall });
            fp.overrides.push_back ({ "width_motion_div", (float) Bar1 });
            out.push_back (fp);
        }
        {
            FactoryPreset fp; fp.name = "Sidechain Bounce";
            fp.overrides.push_back ({ "volume_enabled", 1.0f });
            fp.overrides.push_back ({ "volume_mode", (float) Duck });
            fp.overrides.push_back ({ "volume_base", 55.0f });
            fp.overrides.push_back ({ "volume_modsource", (float) ModLfo });
            fp.overrides.push_back ({ "volume_moddepth", 85.0f });
            fp.overrides.push_back ({ "volume_lfo_shape", (float) Sqr });
            fp.overrides.push_back ({ "volume_lfo_synced", 1.0f });
            fp.overrides.push_back ({ "volume_lfo_div", (float) D1_4 });
            fp.overrides.push_back ({ "width_enabled", 1.0f });
            fp.overrides.push_back ({ "width_mode", (float) StereoWidth });
            fp.overrides.push_back ({ "width_modsource", (float) ModLfo });
            fp.overrides.push_back ({ "width_moddepth", 30.0f });
            fp.overrides.push_back ({ "width_lfo_div", (float) D1_4 });
            out.push_back (fp);
        }

        // ---------- Drums / melody accents / loop chopping ----------
        {
            FactoryPreset fp; fp.name = "Drum Bus Glue";
            fp.overrides.push_back ({ "drive_enabled", 1.0f });
            fp.overrides.push_back ({ "drive_mode", (float) Tube });
            fp.overrides.push_back ({ "drive_base", 30.0f });
            fp.overrides.push_back ({ "drive_mix", 60.0f });
            fp.overrides.push_back ({ "width_enabled", 1.0f });
            fp.overrides.push_back ({ "width_mode", (float) MonoBass });
            fp.overrides.push_back ({ "width_base", 55.0f });
            fp.overrides.push_back ({ "width_crossover", 35.0f });
            out.push_back (fp);
        }
        {
            FactoryPreset fp; fp.name = "Accent Riser";
            fp.overrides.push_back ({ "volume_enabled", 1.0f });
            fp.overrides.push_back ({ "volume_mode", (float) Exponential });
            fp.overrides.push_back ({ "volume_base", 70.0f });
            fp.overrides.push_back ({ "volume_modsource", (float) Motion });
            fp.overrides.push_back ({ "volume_moddepth", 90.0f });
            fp.overrides.push_back ({ "volume_motion_shape", (float) RiseFall });
            fp.overrides.push_back ({ "volume_motion_synced", 1.0f });
            fp.overrides.push_back ({ "volume_motion_div", (float) Bar1 });
            fp.overrides.push_back ({ "pan_enabled", 1.0f });
            fp.overrides.push_back ({ "pan_mode", (float) Rotary });
            fp.overrides.push_back ({ "pan_modsource", (float) Motion });
            fp.overrides.push_back ({ "pan_moddepth", 60.0f });
            fp.overrides.push_back ({ "pan_motion_shape", (float) Ramp });
            fp.overrides.push_back ({ "pan_motion_div", (float) Bar1 });
            out.push_back (fp);
        }
        {
            FactoryPreset fp; fp.name = "Melody Chopper";
            setStutterPattern (fp, 8, D1_8, { SOff, Rep8, SOff, Rep8, SOff, SOff, Rep16, SOff });
            fp.overrides.push_back ({ "stutter_mix", 80.0f });
            fp.overrides.push_back ({ "retro_enabled", 1.0f });
            fp.overrides.push_back ({ "retro_mode", (float) Lossy });
            fp.overrides.push_back ({ "retro_base", 40.0f });
            fp.overrides.push_back ({ "retro_mix", 50.0f });
            out.push_back (fp);
        }
        {
            FactoryPreset fp; fp.name = "Loop Wobble";
            fp.overrides.push_back ({ "retro_enabled", 1.0f });
            fp.overrides.push_back ({ "retro_mode", (float) WearTear });
            fp.overrides.push_back ({ "retro_base", 45.0f });
            fp.overrides.push_back ({ "retro_modsource", (float) ModLfo });
            fp.overrides.push_back ({ "retro_moddepth", 60.0f });
            fp.overrides.push_back ({ "retro_lfo_synced", 0.0f });
            fp.overrides.push_back ({ "retro_lfo_rate", 0.35f });
            fp.overrides.push_back ({ "space_enabled", 1.0f });
            fp.overrides.push_back ({ "space_mode", (float) TapeDelay });
            fp.overrides.push_back ({ "space_base", 30.0f });
            fp.overrides.push_back ({ "space_size", 35.0f });
            fp.overrides.push_back ({ "space_decay", 45.0f });
            out.push_back (fp);
        }

        // ---------- General / creative ----------
        {
            FactoryPreset fp; fp.name = "Shimmer Wash";
            fp.overrides.push_back ({ "space_enabled", 1.0f });
            fp.overrides.push_back ({ "space_mode", (float) Shimmer });
            fp.overrides.push_back ({ "space_base", 55.0f });
            fp.overrides.push_back ({ "space_size", 70.0f });
            fp.overrides.push_back ({ "space_decay", 65.0f });
            fp.overrides.push_back ({ "width_enabled", 1.0f });
            fp.overrides.push_back ({ "width_mode", (float) StereoWidth });
            fp.overrides.push_back ({ "width_base", 75.0f });
            out.push_back (fp);
        }
        {
            FactoryPreset fp; fp.name = "Vinyl Dust";
            fp.overrides.push_back ({ "drive_enabled", 1.0f });
            fp.overrides.push_back ({ "drive_mode", (float) Tape });
            fp.overrides.push_back ({ "drive_base", 35.0f });
            fp.overrides.push_back ({ "retro_enabled", 1.0f });
            fp.overrides.push_back ({ "retro_mode", (float) WearTear });
            fp.overrides.push_back ({ "retro_base", 40.0f });
            fp.overrides.push_back ({ "retro_mix", 55.0f });
            out.push_back (fp);
        }
        {
            FactoryPreset fp; fp.name = "Haas Wide";
            fp.overrides.push_back ({ "width_enabled", 1.0f });
            fp.overrides.push_back ({ "width_mode", (float) Haas });
            fp.overrides.push_back ({ "width_base", 60.0f });
            fp.overrides.push_back ({ "width_modsource", (float) ModLfo });
            fp.overrides.push_back ({ "width_moddepth", 40.0f });
            fp.overrides.push_back ({ "width_lfo_synced", 1.0f });
            fp.overrides.push_back ({ "width_lfo_div", (float) Bar2 });
            out.push_back (fp);
        }
        {
            FactoryPreset fp; fp.name = "Rotary Dream";
            fp.overrides.push_back ({ "pan_enabled", 1.0f });
            fp.overrides.push_back ({ "pan_mode", (float) Rotary });
            fp.overrides.push_back ({ "pan_modsource", (float) ModLfo });
            fp.overrides.push_back ({ "pan_moddepth", 65.0f });
            fp.overrides.push_back ({ "pan_lfo_synced", 0.0f });
            fp.overrides.push_back ({ "pan_lfo_rate", 0.2f });
            fp.overrides.push_back ({ "space_enabled", 1.0f });
            fp.overrides.push_back ({ "space_mode", (float) Hall });
            fp.overrides.push_back ({ "space_base", 30.0f });
            fp.overrides.push_back ({ "space_size", 55.0f });
            out.push_back (fp);
        }

        return out;
    }
}
