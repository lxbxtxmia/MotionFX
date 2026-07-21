#pragma once
#include "DspUtils.h"
#include <array>
#include <vector>

namespace mfx
{
    struct TransportInfo
    {
        double bpm = 120.0;
        double ppqPosition = 0.0;
        bool isPlaying = false;
    };

    // Note divisions used by every tempo-synced source (LFO / sequencer / motion).
    enum class SyncDiv
    {
        d4Bar, d2Bar, d1Bar, d1_2, d1_4, d1_8, d1_8T, d1_16, d1_16T, d1_32
    };

    inline double syncDivToBeats (SyncDiv d) noexcept
    {
        switch (d)
        {
            case SyncDiv::d4Bar:  return 16.0;
            case SyncDiv::d2Bar:  return 8.0;
            case SyncDiv::d1Bar:  return 4.0;
            case SyncDiv::d1_2:   return 2.0;
            case SyncDiv::d1_4:   return 1.0;
            case SyncDiv::d1_8:   return 0.5;
            case SyncDiv::d1_8T:  return 0.5 * (2.0 / 3.0);
            case SyncDiv::d1_16:  return 0.25;
            case SyncDiv::d1_16T: return 0.25 * (2.0 / 3.0);
            case SyncDiv::d1_32:  return 0.125;
        }
        return 1.0;
    }

    inline const char* syncDivLabel (SyncDiv d) noexcept
    {
        switch (d)
        {
            case SyncDiv::d4Bar:  return "4 Bar";
            case SyncDiv::d2Bar:  return "2 Bar";
            case SyncDiv::d1Bar:  return "1 Bar";
            case SyncDiv::d1_2:   return "1/2";
            case SyncDiv::d1_4:   return "1/4";
            case SyncDiv::d1_8:   return "1/8";
            case SyncDiv::d1_8T:  return "1/8T";
            case SyncDiv::d1_16:  return "1/16";
            case SyncDiv::d1_16T: return "1/16T";
            case SyncDiv::d1_32:  return "1/32";
        }
        return "1/4";
    }

    enum class LfoShape { Sine, Triangle, Square, Saw };

    //==============================================================================
    class Lfo
    {
    public:
        void prepare (double sr) noexcept { sampleRate = sr; }
        void reset() noexcept { phase = 0.0; }
        float getPhase() const noexcept { return (float) phase; }

        // rateHz used when synced == false. When synced, transport drives phase.
        void setParams (LfoShape s, bool synced, float rateHz, SyncDiv div, bool bipolarOut) noexcept
        {
            shape = s; isSynced = synced; freeRateHz = juce::jlimit (0.01f, 20.0f, rateHz);
            division = div; bipolar = bipolarOut;
        }

        float process (const TransportInfo& t, int numSamples) noexcept
        {
            if (isSynced)
            {
                const double beats = syncDivToBeats (division);
                phase = std::fmod (t.ppqPosition / beats, 1.0);
                if (phase < 0.0) phase += 1.0;
            }
            else
            {
                phase += (freeRateHz * numSamples) / sampleRate;
                phase -= std::floor (phase);
            }
            return shapeAt (phase);
        }

    private:
        float shapeAt (double p) const noexcept
        {
            float v = 0.0f;
            switch (shape)
            {
                case LfoShape::Sine:     v = 0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * p); break;
                case LfoShape::Triangle: v = (float) (1.0 - std::abs (2.0 * (p - std::floor (p + 0.5)))); break;
                case LfoShape::Square:   v = (p < 0.5) ? 1.0f : 0.0f; break;
                case LfoShape::Saw:      v = (float) p; break;
            }
            return bipolar ? (v * 2.0f - 1.0f) : v;
        }

        double sampleRate = 44100.0, phase = 0.0;
        LfoShape shape = LfoShape::Sine;
        bool isSynced = true, bipolar = false;
        float freeRateHz = 1.0f;
        SyncDiv division = SyncDiv::d1_4;
    };

    //==============================================================================
    class EnvelopeFollower
    {
    public:
        void prepare (double sr) noexcept { sampleRate = sr; }
        void reset() noexcept { env = 0.0f; }
        void setTimes (float attackMs, float releaseMs) noexcept
        {
            attackMsVal = juce::jmax (0.1f, attackMs);
            releaseMsVal = juce::jmax (0.1f, releaseMs);
        }

        // feed with rectified input; numSamples lets this be called once per
        // block (as the other modulators are) while still honouring the
        // attack/release times in real milliseconds rather than per-sample steps.
        float processSample (float in, int numSamples = 1) noexcept
        {
            float rect = std::abs (in);
            float timeMs = (rect > env) ? attackMsVal : releaseMsVal;
            float coeff = std::exp (-(float) numSamples / (0.001f * timeMs * (float) sampleRate));
            env = coeff * env + (1.0f - coeff) * rect;
            return juce::jlimit (0.0f, 1.0f, env);
        }

    private:
        double sampleRate = 44100.0;
        float env = 0.0f, attackMsVal = 10.0f, releaseMsVal = 150.0f;
    };

    //==============================================================================
    // Draggable breakpoint envelope, evaluated over a repeating (or one-shot) cycle.
    // Kept intentionally simple: linear interpolation between sorted points, with
    // an optional post-smoothing one-pole filter to round off the corners.
    struct EnvPoint { float x = 0.0f, y = 1.0f; }; // x,y both normalised 0..1

    enum class MotionMode { Loop, OneShot, Trigger };

    class MotionEnvelope
    {
    public:
        MotionEnvelope()
        {
            setPluckThenRise();
        }

        void setPluckThenRise()
        {
            points = { {0.0f, 1.0f}, {0.15f, 0.05f}, {0.55f, 0.35f}, {1.0f, 0.0f} };
        }
        void setRiseThenFall() { points = { {0.0f, 0.0f}, {0.5f, 1.0f}, {1.0f, 0.0f} }; }
        void setRamp()         { points = { {0.0f, 0.0f}, {1.0f, 1.0f} }; }
        void setDip()          { points = { {0.0f, 1.0f}, {0.5f, 0.0f}, {1.0f, 1.0f} }; }
        void setFlat()         { points = { {0.0f, 1.0f}, {1.0f, 1.0f} }; }

        void setPoints (std::vector<EnvPoint> p) { if (p.size() >= 2) points = std::move (p); }
        const std::vector<EnvPoint>& getPoints() const noexcept { return points; }

        void prepare (double sr) noexcept { sampleRate = sr; smoother.z = 0.0f; }
        void reset() noexcept { phase = 0.0; triggered = false; }
        float getPhase() const noexcept { return (float) phase; }

        void setParams (MotionMode m, bool synced, float rateHz, SyncDiv div, float smoothAmount) noexcept
        {
            mode = m; isSynced = synced; freeRateHz = juce::jlimit (0.01f, 20.0f, rateHz);
            division = div; smoothCoeff = juce::jlimit (0.0f, 0.999f, smoothAmount);
        }

        void trigger() noexcept { if (mode == MotionMode::Trigger) { phase = 0.0; triggered = true; } }

        float process (const TransportInfo& t, int numSamples) noexcept
        {
            if (mode == MotionMode::Trigger && ! triggered)
            {
                // hold at the envelope's resting value until (re)triggered
                float held = evaluate (points.back().x);
                return smoother.process (held, smoothCoeff);
            }

            if (isSynced)
            {
                const double beats = syncDivToBeats (division);
                double p = std::fmod (t.ppqPosition / beats, 1.0);
                if (p < 0.0) p += 1.0;
                phase = p;
            }
            else
            {
                phase += (freeRateHz * numSamples) / sampleRate;
                if (phase >= 1.0)
                {
                    if (mode == MotionMode::OneShot || mode == MotionMode::Trigger) { phase = 1.0; triggered = false; }
                    else phase -= std::floor (phase);
                }
            }

            float raw = evaluate ((float) phase);
            return smoother.process (raw, smoothCoeff);
        }

    private:
        float evaluate (float x) const noexcept
        {
            if (points.size() < 2) return 0.0f;
            for (size_t i = 0; i + 1 < points.size(); ++i)
            {
                const auto& a = points[i];
                const auto& b = points[i + 1];
                if (x >= a.x && x <= b.x)
                {
                    float span = juce::jmax (0.0001f, b.x - a.x);
                    float t = (x - a.x) / span;
                    return a.y + t * (b.y - a.y);
                }
            }
            return points.back().y;
        }

        std::vector<EnvPoint> points;
        double sampleRate = 44100.0, phase = 0.0;
        MotionMode mode = MotionMode::Loop;
        bool isSynced = true, triggered = false;
        float freeRateHz = 1.0f, smoothCoeff = 0.0f;
        SyncDiv division = SyncDiv::d1Bar;
        OnePole smoother;
    };

    //==============================================================================
    class StepSequencer
    {
    public:
        StepSequencer() { steps.fill (1.0f); }

        void setNumSteps (int n) noexcept { numSteps = juce::jlimit (1, maxSteps, n); }
        int getNumSteps() const noexcept { return numSteps; }
        void setStep (int i, float v) noexcept { if (i >= 0 && i < maxSteps) steps[(size_t) i] = juce::jlimit (0.0f, 1.0f, v); }
        float getStep (int i) const noexcept { return (i >= 0 && i < maxSteps) ? steps[(size_t) i] : 0.0f; }

        void prepare (double sr) noexcept { sampleRate = sr; }
        void setParams (bool synced, float rateHz, SyncDiv div, float smoothMs) noexcept
        {
            isSynced = synced; freeRateHz = juce::jlimit (0.01f, 20.0f, rateHz); division = div;
            smoothTimeMs = juce::jmax (0.0f, smoothMs);
        }

        float process (const TransportInfo& t, int numSamples) noexcept
        {
            double stepPhase; // 0..numSteps continuous position
            if (isSynced)
            {
                const double beats = syncDivToBeats (division);
                stepPhase = std::fmod (t.ppqPosition / beats, (double) numSteps);
                if (stepPhase < 0.0) stepPhase += numSteps;
            }
            else
            {
                freePos += (freeRateHz * numSamples) / sampleRate;
                stepPhase = std::fmod (freePos, (double) numSteps);
            }

            int idx = (int) stepPhase;
            currentStepIndex = idx;
            float target = getStep (idx);

            if (smoothTimeMs <= 0.01f)
            {
                current = target;
            }
            else
            {
                float coeff = std::exp (-((float) numSamples / (float) sampleRate) / (0.001f * smoothTimeMs));
                current = coeff * current + (1.0f - coeff) * target;
            }
            return current;
        }

        static constexpr int maxSteps = 32;
        int getCurrentStepIndex() const noexcept { return currentStepIndex; }

    private:
        std::array<float, maxSteps> steps {};
        int numSteps = 8;
        double sampleRate = 44100.0, freePos = 0.0;
        bool isSynced = true;
        float freeRateHz = 2.0f, smoothTimeMs = 2.0f;
        SyncDiv division = SyncDiv::d1_8;
        float current = 0.0f;
        int currentStepIndex = 0;
    };

    //==============================================================================
    enum class ModSourceType { Off, Lfo, EnvFollower, Motion, Sequencer };

    // Bundles all four engines for one effect slot; only the selected one is
    // advanced/queried each block, the rest stay idle (cheap).
    struct ModulationUnit
    {
        ModSourceType source = ModSourceType::Off;
        float depth = 0.0f; // 0..1 amount applied to the target parameter

        Lfo lfo;
        EnvelopeFollower env;
        MotionEnvelope motion;
        StepSequencer seq;

        void prepare (double sr)
        {
            lfo.prepare (sr); env.prepare (sr); motion.prepare (sr); seq.prepare (sr);
        }
        void reset() { lfo.reset(); env.reset(); motion.reset(); }

        // sidechainSample: rectified input level for the envelope follower.
        // Returns modulation value in 0..1 range (callers decide bipolar mapping
        // if their target needs it, using *2-1 on the raw source before storing here
        // is avoided -- Lfo/Motion already expose a bipolar flag at setParams time).
        float getValue (const TransportInfo& t, int numSamples, float sidechainSample) noexcept
        {
            switch (source)
            {
                case ModSourceType::Lfo:         return lfo.process (t, numSamples);
                case ModSourceType::EnvFollower: return env.processSample (sidechainSample, numSamples);
                case ModSourceType::Motion:       return motion.process (t, numSamples);
                case ModSourceType::Sequencer:    return seq.process (t, numSamples);
                case ModSourceType::Off: default: return 0.0f;
            }
        }
    };
}
