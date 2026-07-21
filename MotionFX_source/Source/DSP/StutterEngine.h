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
            const int cap = (int) (2.0 * sr);
            for (auto& b : circBuf) b.assign ((size_t) cap, 0.0f);
            for (auto& s : snippetBuf) s.assign ((size_t) cap, 0.0f);
            capacity = cap;
            reset();
        }

        void reset() noexcept
        {
            writePos = 0; lastStepIndex = -1; sampleCounterInStep = 0;
            playPos = 0.0; rate = 1.0; loopSnippetLen = 1; loopCounter = 0;
            currentAction = StepAction::Off;
            for (auto& b : circBuf) std::fill (b.begin(), b.end(), 0.0f);
        }

        void setEnabled (bool e) noexcept { enabled = e; }
        void setPattern (int n, SyncDiv d) noexcept { numSteps = juce::jlimit (1, maxSteps, n); division = d; }
        void setStepAction (int i, StepAction a) noexcept { if (i >= 0 && i < maxSteps) pattern[(size_t) i] = a; }
        StepAction getStepAction (int i) const noexcept { return (i >= 0 && i < maxSteps) ? pattern[(size_t) i] : StepAction::Off; }
        void setMix (float m) noexcept { mix = juce::jlimit (0.0f, 1.0f, m); }

        void processBlock (juce::AudioBuffer<float>& buf, const TransportInfo& t) noexcept
        {
            if (! enabled || buf.getNumChannels() < 2) return;

            const double beatsPerStep = syncDivToBeats (division);
            const double secPerBeat = 60.0 / juce::jmax (1.0, t.bpm);
            const double stepLenSamplesD = beatsPerStep * secPerBeat * sampleRate;
            const int stepLenSamples = juce::jmax (4, (int) stepLenSamplesD);
            const int fadeLen = juce::jmin (stepLenSamples / 4, (int) (0.003 * sampleRate) + 1);

            double stepPhaseCont = t.ppqPosition / beatsPerStep;
            stepPhaseCont = std::fmod (stepPhaseCont, (double) numSteps);
            if (stepPhaseCont < 0.0) stepPhaseCont += numSteps;

            auto* L = buf.getWritePointer (0);
            auto* R = buf.getWritePointer (1);
            const int n = buf.getNumSamples();

            for (int i = 0; i < n; ++i)
            {
                // advance the continuous step-phase sample-by-sample so step
                // boundaries land precisely regardless of block size.
                double instantaneousStepPos = stepPhaseCont + (double) i * (1.0 / stepLenSamplesD);
                int stepIndex = (int) std::floor (instantaneousStepPos) % numSteps;
                if (stepIndex < 0) stepIndex += numSteps;

                if (stepIndex != lastStepIndex)
                {
                    lastStepIndex = stepIndex;
                    sampleCounterInStep = 0;
                    currentAction = pattern[(size_t) stepIndex];
                    beginStep (currentAction, stepLenSamples);
                }

                float liveL = L[i], liveR = R[i];
                circBuf[0][(size_t) writePos] = liveL;
                circBuf[1][(size_t) writePos] = liveR;

                float procL = liveL, procR = liveR;
                bool isOff = (currentAction == StepAction::Off);

                if (! isOff)
                    computeSample (procL, procR, liveL, liveR);

                float fadeGain = 1.0f;
                if (! isOff)
                {
                    if (sampleCounterInStep < fadeLen)
                        fadeGain = (float) sampleCounterInStep / (float) fadeLen;
                    else if (sampleCounterInStep > stepLenSamples - fadeLen)
                        fadeGain = juce::jmax (0.0f, (float) (stepLenSamples - sampleCounterInStep) / (float) fadeLen);
                }

                float outL = isOff ? liveL : (liveL * (1.0f - fadeGain) + procL * fadeGain);
                float outR = isOff ? liveR : (liveR * (1.0f - fadeGain) + procR * fadeGain);

                L[i] = flushDenorm (liveL * (1.0f - mix) + outL * mix);
                R[i] = flushDenorm (liveR * (1.0f - mix) + outR * mix);

                writePos = (writePos + 1) % capacity;
                ++sampleCounterInStep;
            }
        }

        // exposed for the UI to draw + edit
        int getNumSteps() const noexcept { return numSteps; }
        SyncDiv getDivision() const noexcept { return division; }
        int getCurrentStepIndex() const noexcept { return lastStepIndex; }

    private:
        void beginStep (StepAction action, int stepLenSamples) noexcept
        {
            switch (action)
            {
                case StepAction::Repeat4:  loopSnippetLen = juce::jlimit (2, capacity - 4, stepLenSamples / 4); captureSnippet (loopSnippetLen, false); loopCounter = 0; break;
                case StepAction::Repeat8:  loopSnippetLen = juce::jlimit (2, capacity - 4, stepLenSamples / 8); captureSnippet (loopSnippetLen, false); loopCounter = 0; break;
                case StepAction::Repeat16: loopSnippetLen = juce::jlimit (2, capacity - 4, stepLenSamples / 16); captureSnippet (loopSnippetLen, false); loopCounter = 0; break;
                case StepAction::Repeat32: loopSnippetLen = juce::jlimit (2, capacity - 4, stepLenSamples / 32); captureSnippet (loopSnippetLen, false); loopCounter = 0; break;
                case StepAction::Reverse:  loopSnippetLen = juce::jlimit (2, capacity - 4, stepLenSamples); captureSnippet (loopSnippetLen, true); loopCounter = 0; break;
                case StepAction::TapeStopDown: playPos = (double) writePos; rate = 1.0; break;
                case StepAction::TapeStopUp:   playPos = (double) writePos - 8.0; rate = 0.0; break;
                case StepAction::PitchUp:      playPos = (double) writePos; rate = 1.6; loopSnippetLen = juce::jmax (4, stepLenSamples); break;
                case StepAction::PitchDown:    playPos = (double) writePos; rate = 0.6; break;
                case StepAction::Off:
                case StepAction::Gate: default: break;
            }
            currentStepLenSamples = stepLenSamples;
        }

        void captureSnippet (int len, bool reversed) noexcept
        {
            // writePos is the slot about to be written this sample, so the most
            // recently completed sample sits one behind it.
            int mostRecent = (writePos - 1 + capacity) % capacity;
            for (int ch = 0; ch < 2; ++ch)
            {
                for (int k = 0; k < len; ++k)
                {
                    int srcIdx = reversed ? (mostRecent - k + capacity) % capacity
                                           : (mostRecent - len + 1 + k + capacity) % capacity;
                    snippetBuf[(size_t) ch][(size_t) k] = circBuf[(size_t) ch][(size_t) srcIdx];
                }
            }
        }

        float interpRead (int ch, double pos) const noexcept
        {
            pos = std::fmod (pos, (double) capacity);
            while (pos < 0.0) pos += capacity;
            int i0 = (int) pos;
            int i1 = (i0 + 1) % capacity;
            float frac = (float) (pos - std::floor (pos));
            const auto& b = circBuf[(size_t) ch];
            return b[(size_t) i0] + frac * (b[(size_t) i1] - b[(size_t) i0]);
        }

        void computeSample (float& procL, float& procR, float liveL, float liveR) noexcept
        {
            juce::ignoreUnused (liveL, liveR);
            switch (currentAction)
            {
                case StepAction::Repeat4: case StepAction::Repeat8:
                case StepAction::Repeat16: case StepAction::Repeat32:
                {
                    int idx = loopCounter % loopSnippetLen;
                    procL = snippetBuf[0][(size_t) idx];
                    procR = snippetBuf[1][(size_t) idx];
                    ++loopCounter;
                    break;
                }
                case StepAction::Reverse:
                {
                    int idx = juce::jmin (loopCounter, loopSnippetLen - 1);
                    procL = snippetBuf[0][(size_t) idx];
                    procR = snippetBuf[1][(size_t) idx];
                    ++loopCounter;
                    break;
                }
                case StepAction::TapeStopDown:
                {
                    float frac = juce::jlimit (0.0f, 1.0f, (float) sampleCounterInStep / (float) currentStepLenSamples);
                    rate = juce::jmax (0.0, 1.0 - (double) frac);
                    playPos += rate;
                    procL = interpRead (0, playPos); procR = interpRead (1, playPos);
                    break;
                }
                case StepAction::TapeStopUp:
                {
                    float frac = juce::jlimit (0.0f, 1.0f, (float) sampleCounterInStep / (float) currentStepLenSamples);
                    rate = (double) frac;
                    playPos += rate;
                    procL = interpRead (0, playPos); procR = interpRead (1, playPos);
                    break;
                }
                case StepAction::PitchUp:
                {
                    playPos += rate;
                    if ((double) writePos - playPos < 32.0)
                        playPos -= loopSnippetLen;
                    procL = interpRead (0, playPos); procR = interpRead (1, playPos);
                    break;
                }
                case StepAction::PitchDown:
                {
                    playPos += rate;
                    procL = interpRead (0, playPos); procR = interpRead (1, playPos);
                    break;
                }
                case StepAction::Gate: procL = 0.0f; procR = 0.0f; break;
                case StepAction::Off: default: break;
            }
        }

        double sampleRate = 44100.0;
        int capacity = 88200;
        std::array<std::vector<float>, 2> circBuf, snippetBuf;
        int writePos = 0;

        bool enabled = false;
        int numSteps = 16;
        SyncDiv division = SyncDiv::d1_16;
        std::array<StepAction, maxSteps> pattern {};
        float mix = 1.0f;

        int lastStepIndex = -1;
        int sampleCounterInStep = 0;
        int currentStepLenSamples = 4410;
        StepAction currentAction = StepAction::Off;

        double playPos = 0.0, rate = 1.0;
        int loopSnippetLen = 1, loopCounter = 0;
    };
}
