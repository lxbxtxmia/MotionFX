#pragma once
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
