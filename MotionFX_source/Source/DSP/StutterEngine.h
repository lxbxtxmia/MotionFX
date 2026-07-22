#pragma once
#include "Modulation.h"
#include <array>
#include <vector>
#include <cmath>

namespace mfx
{
    enum class RepeatAction { Off, Quarter, Eighth, Sixteenth, ThirtySecond };
    enum class TapeAction { Off, Down, Up };

    class StereoHistory
    {
    public:
        void prepare (double sampleRate, double seconds)
        {
            capacity = juce::jmax (64, (int) std::ceil (sampleRate * seconds));
            for (auto& channel : data)
                channel.assign ((size_t) capacity, 0.0f);
            reset();
        }

        void reset() noexcept
        {
            writePosition = 0;
            validSamples = 0;
            for (auto& channel : data)
                std::fill (channel.begin(), channel.end(), 0.0f);
        }

        void push (float left, float right) noexcept
        {
            data[0][(size_t) writePosition] = left;
            data[1][(size_t) writePosition] = right;
            writePosition = (writePosition + 1) % capacity;
            validSamples = juce::jmin (capacity, validSamples + 1);
        }

        float read (int channel, double position) const noexcept
        {
            if (capacity <= 0)
                return 0.0f;

            position = std::fmod (position, (double) capacity);
            while (position < 0.0)
                position += capacity;

            const int first = (int) std::floor (position) % capacity;
            const int second = (first + 1) % capacity;
            const float fraction = (float) (position - std::floor (position));
            const auto& samples = data[(size_t) channel];
            return samples[(size_t) first]
                 + fraction * (samples[(size_t) second] - samples[(size_t) first]);
        }

        int captureRecent (std::array<std::vector<float>, 2>& destination,
                           int requestedLength, bool reversed) const noexcept
        {
            const int available = juce::jmax (2, juce::jmin (validSamples, capacity - 4));
            const int length = juce::jlimit (2, available, requestedLength);
            const int mostRecent = (writePosition - 1 + capacity) % capacity;

            for (int channel = 0; channel < 2; ++channel)
            {
                for (int sample = 0; sample < length; ++sample)
                {
                    const int source = reversed
                        ? (mostRecent - sample + capacity) % capacity
                        : (mostRecent - length + 1 + sample + capacity) % capacity;
                    destination[(size_t) channel][(size_t) sample]
                        = data[(size_t) channel][(size_t) source];
                }
            }
            return length;
        }

        double mostRecentPosition() const noexcept
        {
            return (double) ((writePosition - 1 + capacity) % capacity);
        }

        int getCapacity() const noexcept { return capacity; }
        int getValidSamples() const noexcept { return validSamples; }
        int getWritePosition() const noexcept { return writePosition; }

    private:
        std::array<std::vector<float>, 2> data;
        int capacity = 64;
        int writePosition = 0;
        int validSamples = 0;
    };

    class StutterRepeatProcessor
    {
    public:
        void prepare (double sr)
        {
            sampleRate = sr;
            history.prepare (sr, 8.0);
            const int capacity = history.getCapacity();
            for (auto& channel : snippet)
                channel.assign ((size_t) capacity, 0.0f);
            reset();
        }

        void reset() noexcept
        {
            history.reset();
            active = false;
            snippetLength = 2;
            playCounter = 0;
        }

        void beginStep (RepeatAction action, double bpm) noexcept
        {
            active = action != RepeatAction::Off;
            playCounter = 0;
            if (! active)
                return;

            snippetLength = history.captureRecent (
                snippet, repeatLengthSamples (action, bpm), false);
        }

        void processSample (float inputLeft, float inputRight, float fade,
                            float& outputLeft, float& outputRight) noexcept
        {
            outputLeft = inputLeft;
            outputRight = inputRight;

            if (active)
            {
                const float wetLeft = readLoop (0, playCounter);
                const float wetRight = readLoop (1, playCounter);
                outputLeft = juce::jmap (fade, inputLeft, wetLeft);
                outputRight = juce::jmap (fade, inputRight, wetRight);
                ++playCounter;
            }

            history.push (inputLeft, inputRight);
        }

        int getNominalLengthSamples (RepeatAction action, double bpm) const noexcept
        {
            return repeatLengthSamples (action, bpm);
        }

    private:
        int repeatLengthSamples (RepeatAction action, double bpm) const noexcept
        {
            double beats = 1.0;
            switch (action)
            {
                case RepeatAction::Quarter: beats = 1.0; break;
                case RepeatAction::Eighth: beats = 0.5; break;
                case RepeatAction::Sixteenth: beats = 0.25; break;
                case RepeatAction::ThirtySecond: beats = 0.125; break;
                case RepeatAction::Off: break;
            }

            const double seconds = beats * 60.0 / juce::jmax (1.0, bpm);
            return juce::jlimit (2, juce::jmax (2, history.getCapacity() - 4),
                                 (int) std::round (seconds * sampleRate));
        }

        float readLoop (int channel, int counter) const noexcept
        {
            const int index = counter % juce::jmax (2, snippetLength);
            const int crossfade = juce::jmin (
                snippetLength / 4,
                juce::jmax (1, (int) (0.002 * sampleRate)));

            if (index >= crossfade || crossfade <= 1)
                return snippet[(size_t) channel][(size_t) index];

            const int previous = snippetLength - crossfade + index;
            const float alpha = (float) index / (float) crossfade;
            return juce::jmap (
                alpha,
                snippet[(size_t) channel][(size_t) previous],
                snippet[(size_t) channel][(size_t) index]);
        }

        double sampleRate = 44100.0;
        StereoHistory history;
        std::array<std::vector<float>, 2> snippet;
        int snippetLength = 2;
        int playCounter = 0;
        bool active = false;
    };

    class StutterReverseProcessor
    {
    public:
        void prepare (double sr)
        {
            history.prepare (sr, 8.0);
            const int capacity = history.getCapacity();
            for (auto& channel : snippet)
                channel.assign ((size_t) capacity, 0.0f);
            reset();
        }

        void reset() noexcept
        {
            history.reset();
            active = false;
            snippetLength = 2;
            playCounter = 0;
        }

        void beginStep (bool shouldBeActive, int stepLengthSamples) noexcept
        {
            active = shouldBeActive;
            playCounter = 0;
            if (active)
                snippetLength = history.captureRecent (snippet, stepLengthSamples, true);
        }

        void processSample (float inputLeft, float inputRight, float fade,
                            float& outputLeft, float& outputRight) noexcept
        {
            outputLeft = inputLeft;
            outputRight = inputRight;

            if (active)
            {
                const int index = playCounter % juce::jmax (2, snippetLength);
                outputLeft = juce::jmap (
                    fade, inputLeft, snippet[0][(size_t) index]);
                outputRight = juce::jmap (
                    fade, inputRight, snippet[1][(size_t) index]);
                ++playCounter;
            }

            history.push (inputLeft, inputRight);
        }

    private:
        StereoHistory history;
        std::array<std::vector<float>, 2> snippet;
        int snippetLength = 2;
        int playCounter = 0;
        bool active = false;
    };

    class StutterTapeProcessor
    {
    public:
        void prepare (double sr)
        {
            history.prepare (sr, 8.0);
            reset();
        }

        void reset() noexcept
        {
            history.reset();
            active = false;
            action = TapeAction::Off;
            playPosition = 0.0;
            stepLength = 1;
            sampleCounter = 0;
        }

        void beginStep (TapeAction newAction, int stepLengthSamples) noexcept
        {
            action = newAction;
            active = action != TapeAction::Off;
            playPosition = history.mostRecentPosition();
            stepLength = juce::jmax (1, stepLengthSamples);
            sampleCounter = 0;
        }

        void processSample (float inputLeft, float inputRight, float fade,
                            float& outputLeft, float& outputRight) noexcept
        {
            outputLeft = inputLeft;
            outputRight = inputRight;

            if (active && history.getValidSamples() > 8)
            {
                const float progress = juce::jlimit (
                    0.0f, 1.0f,
                    (float) sampleCounter / (float) stepLength);
                const double rate = action == TapeAction::Down
                    ? juce::jmax (0.0, 1.0 - (double) progress)
                    : (double) progress * 2.0;

                const float wetLeft = history.read (0, playPosition);
                const float wetRight = history.read (1, playPosition);
                outputLeft = juce::jmap (fade, inputLeft, wetLeft);
                outputRight = juce::jmap (fade, inputRight, wetRight);
                playPosition += rate;
                ++sampleCounter;
            }

            history.push (inputLeft, inputRight);
        }

    private:
        StereoHistory history;
        TapeAction action = TapeAction::Off;
        double playPosition = 0.0;
        int stepLength = 1;
        int sampleCounter = 0;
        bool active = false;
    };

    class StutterPitchProcessor
    {
    public:
        void prepare (double sr)
        {
            sampleRate = sr;
            capacity = juce::jmax (4096, (int) std::ceil (sr * 0.5));
            for (auto& channel : buffer)
                channel.assign ((size_t) capacity, 0.0f);
            reset();
        }

        void reset() noexcept
        {
            for (auto& channel : buffer)
                std::fill (channel.begin(), channel.end(), 0.0f);
            writePosition = 0;
            validSamples = 0;
            phase = 0.0;
        }

        void setParameters (float newGrainMilliseconds) noexcept
        {
            grainMilliseconds = juce::jlimit (10.0f, 120.0f, newGrainMilliseconds);
        }

        static double semitonesToRatio (int value) noexcept
        {
            return std::pow (2.0, (double) value / 12.0);
        }

        void processSample (float inputLeft, float inputRight, bool active,
                            int semitoneValue, float fade,
                            float& outputLeft, float& outputRight) noexcept
        {
            buffer[0][(size_t) writePosition] = inputLeft;
            buffer[1][(size_t) writePosition] = inputRight;

            outputLeft = inputLeft;
            outputRight = inputRight;

            const int grainSamples = juce::jlimit (
                64, capacity / 3,
                (int) std::round (grainMilliseconds * 0.001 * sampleRate));

            const int clampedSemitones = juce::jlimit (-24, 24, semitoneValue);
            if (active && clampedSemitones != 0 && validSamples > grainSamples + 16)
            {
                const double ratio = semitonesToRatio (clampedSemitones);
                const double phaseIncrement = (1.0 - ratio) / (double) grainSamples;
                phase = wrap01 (phase + phaseIncrement);
                const double secondPhase = wrap01 (phase + 0.5);

                const double minimumDelay = 8.0;
                const double delayA = minimumDelay + phase * grainSamples;
                const double delayB = minimumDelay + secondPhase * grainSamples;

                const float windowA = hann ((float) phase);
                const float windowB = hann ((float) secondPhase);
                const float normaliser = 1.0f / juce::jmax (0.0001f, windowA + windowB);

                const float wetLeft = (
                    read (0, (double) writePosition - delayA) * windowA
                    + read (0, (double) writePosition - delayB) * windowB) * normaliser;
                const float wetRight = (
                    read (1, (double) writePosition - delayA) * windowA
                    + read (1, (double) writePosition - delayB) * windowB) * normaliser;

                outputLeft = juce::jmap (fade, inputLeft, wetLeft);
                outputRight = juce::jmap (fade, inputRight, wetRight);
            }

            writePosition = (writePosition + 1) % capacity;
            validSamples = juce::jmin (capacity, validSamples + 1);
        }

    private:
        static double wrap01 (double value) noexcept
        {
            value -= std::floor (value);
            return value;
        }

        static float hann (float phaseValue) noexcept
        {
            return 0.5f - 0.5f * std::cos (
                juce::MathConstants<float>::twoPi * phaseValue);
        }

        float read (int channel, double position) const noexcept
        {
            position = std::fmod (position, (double) capacity);
            while (position < 0.0)
                position += capacity;

            const int first = (int) std::floor (position) % capacity;
            const int second = (first + 1) % capacity;
            const float fraction = (float) (position - std::floor (position));
            const auto& samples = buffer[(size_t) channel];
            return samples[(size_t) first]
                 + fraction * (samples[(size_t) second] - samples[(size_t) first]);
        }

        double sampleRate = 44100.0;
        int capacity = 22050;
        std::array<std::vector<float>, 2> buffer;
        int writePosition = 0;
        int validSamples = 0;
        double phase = 0.0;
        float grainMilliseconds = 50.0f;
    };

    class StutterGateProcessor
    {
    public:
        void processSample (float inputLeft, float inputRight, bool active, float fade,
                            float& outputLeft, float& outputRight) const noexcept
        {
            const float gain = active ? 1.0f - fade : 1.0f;
            outputLeft = inputLeft * gain;
            outputRight = inputRight * gain;
        }
    };

    class StutterEngine
    {
    public:
        static constexpr int maxSteps = 32;

        void prepare (double sr)
        {
            sampleRate = sr;
            repeat.prepare (sr);
            reverse.prepare (sr);
            tape.prepare (sr);
            pitch.prepare (sr);
            reset();
        }

        void reset() noexcept
        {
            repeat.reset();
            reverse.reset();
            tape.reset();
            pitch.reset();
            lastStepIndex = -1;
            sampleCounterInStep = 0;
            currentStepLengthSamples = 1;
        }

        void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }
        void setPattern (int steps, SyncDiv stepDivision) noexcept
        {
            numSteps = juce::jlimit (1, maxSteps, steps);
            division = stepDivision;
        }
        void setMix (float newMix) noexcept { mix = juce::jlimit (0.0f, 1.0f, newMix); }
        void setPitchGrainMilliseconds (float grainMs) noexcept
        {
            pitch.setParameters (grainMs);
        }

        void setRepeatStep (int index, RepeatAction action) noexcept
        {
            if (index >= 0 && index < maxSteps)
                repeatPattern[(size_t) index] = action;
        }
        void setReverseStep (int index, bool active) noexcept
        {
            if (index >= 0 && index < maxSteps)
                reversePattern[(size_t) index] = active;
        }
        void setTapeStep (int index, TapeAction action) noexcept
        {
            if (index >= 0 && index < maxSteps)
                tapePattern[(size_t) index] = action;
        }
        void setPitchStep (int index, bool active, int semitones) noexcept
        {
            if (index >= 0 && index < maxSteps)
            {
                pitchPattern[(size_t) index] = active;
                pitchSemitones[(size_t) index] = juce::jlimit (-24, 24, semitones);
            }
        }

        bool isPitchStepActive (int index) const noexcept
        {
            return index >= 0 && index < maxSteps
                ? pitchPattern[(size_t) index] : false;
        }

        int getPitchSemitonesForStep (int index) const noexcept
        {
            return index >= 0 && index < maxSteps
                ? pitchSemitones[(size_t) index] : 0;
        }
        void setGateStep (int index, bool active) noexcept
        {
            if (index >= 0 && index < maxSteps)
                gatePattern[(size_t) index] = active;
        }

        int getCurrentStepIndex() const noexcept { return lastStepIndex; }
        int getNumSteps() const noexcept { return numSteps; }
        SyncDiv getDivision() const noexcept { return division; }

        int getNominalRepeatLengthSamples (RepeatAction action, double bpm) const noexcept
        {
            return repeat.getNominalLengthSamples (action, bpm);
        }

        static double pitchRatioForSemitones (int semitones) noexcept
        {
            return StutterPitchProcessor::semitonesToRatio (semitones);
        }

        void processBlock (juce::AudioBuffer<float>& buffer,
                           const TransportInfo& transport) noexcept
        {
            if (! enabled || buffer.getNumChannels() < 2)
                return;

            const double beatsPerStep = syncDivToBeats (division);
            const double secondsPerBeat = 60.0 / juce::jmax (1.0, transport.bpm);
            const double exactStepSamples = beatsPerStep * secondsPerBeat * sampleRate;
            const int stepSamples = juce::jmax (4, (int) std::round (exactStepSamples));

            double continuousPosition = transport.ppqPosition / beatsPerStep;
            continuousPosition = std::fmod (continuousPosition, (double) numSteps);
            if (continuousPosition < 0.0)
                continuousPosition += numSteps;

            auto* left = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);
            const int numSamples = buffer.getNumSamples();

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const double instantaneousPosition = continuousPosition
                    + (double) sample / exactStepSamples;
                int stepIndex = (int) std::floor (instantaneousPosition) % numSteps;
                if (stepIndex < 0)
                    stepIndex += numSteps;

                if (stepIndex != lastStepIndex)
                {
                    lastStepIndex = stepIndex;
                    sampleCounterInStep = 0;
                    currentStepLengthSamples = stepSamples;
                    repeat.beginStep (repeatPattern[(size_t) stepIndex], transport.bpm);
                    reverse.beginStep (reversePattern[(size_t) stepIndex], stepSamples);
                    tape.beginStep (tapePattern[(size_t) stepIndex], stepSamples);
                }

                const float liveLeft = left[sample];
                const float liveRight = right[sample];
                const float fade = stepEffectFade (sampleCounterInStep, stepSamples);

                float stageLeft = liveLeft;
                float stageRight = liveRight;
                float nextLeft = stageLeft;
                float nextRight = stageRight;

                repeat.processSample (stageLeft, stageRight, fade, nextLeft, nextRight);
                stageLeft = nextLeft;
                stageRight = nextRight;

                reverse.processSample (stageLeft, stageRight, fade, nextLeft, nextRight);
                stageLeft = nextLeft;
                stageRight = nextRight;

                tape.processSample (stageLeft, stageRight, fade, nextLeft, nextRight);
                stageLeft = nextLeft;
                stageRight = nextRight;

                pitch.processSample (stageLeft, stageRight,
                                     pitchPattern[(size_t) stepIndex],
                                     pitchSemitones[(size_t) stepIndex], fade,
                                     nextLeft, nextRight);
                stageLeft = nextLeft;
                stageRight = nextRight;

                gate.processSample (stageLeft, stageRight,
                                    gatePattern[(size_t) stepIndex], fade,
                                    nextLeft, nextRight);

                left[sample] = flushDenorm (juce::jmap (mix, liveLeft, nextLeft));
                right[sample] = flushDenorm (juce::jmap (mix, liveRight, nextRight));
                ++sampleCounterInStep;
            }
        }

    private:
        float stepEffectFade (int position, int length) const noexcept
        {
            const int fadeSamples = juce::jmin (
                length / 4,
                juce::jmax (1, (int) (0.003 * sampleRate)));

            if (position < fadeSamples)
                return (float) position / (float) fadeSamples;
            if (position > length - fadeSamples)
                return juce::jlimit (
                    0.0f, 1.0f,
                    (float) (length - position) / (float) fadeSamples);
            return 1.0f;
        }

        double sampleRate = 44100.0;
        bool enabled = false;
        int numSteps = 16;
        SyncDiv division = SyncDiv::d1_16;
        float mix = 1.0f;

        std::array<RepeatAction, maxSteps> repeatPattern {};
        std::array<bool, maxSteps> reversePattern {};
        std::array<TapeAction, maxSteps> tapePattern {};
        std::array<bool, maxSteps> pitchPattern {};
        std::array<int, maxSteps> pitchSemitones {};
        std::array<bool, maxSteps> gatePattern {};

        StutterRepeatProcessor repeat;
        StutterReverseProcessor reverse;
        StutterTapeProcessor tape;
        StutterPitchProcessor pitch;
        StutterGateProcessor gate;

        int lastStepIndex = -1;
        int sampleCounterInStep = 0;
        int currentStepLengthSamples = 1;
    };
}
