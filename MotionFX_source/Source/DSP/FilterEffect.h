#pragma once
#include "DspUtils.h"
#include <array>
#include <vector>

namespace mfx
{
    enum class FilterMode { LowPass, HighPass, BandPass, Notch, Peak, Comb };

    class FilterEffect
    {
    public:
        static constexpr float maximumResonanceBoostDb = 12.0f;

        static float resonanceQForStages (float normalisedResonance,
                                          int numberOfStages) noexcept
        {
            const int stages = juce::jlimit (1, 4, numberOfStages);
            const float shaped = std::pow (
                juce::jlimit (0.0f, 1.0f, normalisedResonance), 2.0f);

            const float maximumMagnitude = std::pow (
                10.0f,
                maximumResonanceBoostDb / (20.0f * (float) stages));
            const float magnitudeSquared =
                maximumMagnitude * maximumMagnitude;

            // Exact inverse of the resonant LP/HP biquad peak:
            // Mpeak = Q / sqrt (1 - 1 / (4 Q^2)).
            const float qSquared = 0.5f * (
                magnitudeSquared
                + std::sqrt (magnitudeSquared * magnitudeSquared
                             - magnitudeSquared));
            const float maximumQ = std::sqrt (qSquared);

            return juce::jmap (
                shaped, 0.0f, 1.0f, 0.70710678f, maximumQ);
        }

        static float peakGainDbPerStage (float normalisedResonance,
                                         int numberOfStages) noexcept
        {
            const int stages = juce::jlimit (1, 4, numberOfStages);
            const float shaped = std::pow (
                juce::jlimit (0.0f, 1.0f, normalisedResonance), 2.0f);
            return maximumResonanceBoostDb * shaped / (float) stages;
        }

        void prepare (double sampleRateToUse, int) noexcept
        {
            sampleRate = juce::jmax (1.0, sampleRateToUse);
            const int maxCombSamples = juce::jmax (
                32, (int) std::ceil (sampleRate * 0.25));

            for (auto& buffer : combBuffer)
                buffer.assign ((size_t) maxCombSamples, 0.0f);

            combCapacity = maxCombSamples;
            mixSm.reset (sampleRate, 15.0f, 1.0f);
            reset();
        }

        void reset() noexcept
        {
            for (auto& channel : biquads)
                for (auto& stage : channel)
                    stage.reset();

            for (auto& buffer : combBuffer)
                std::fill (buffer.begin(), buffer.end(), 0.0f);

            combWritePos = 0;
        }

        void setMode (FilterMode newMode) noexcept { mode = newMode; }

        void setParams (float resonanceNormalised,
                        int slopeChoice,
                        float mixNormalised) noexcept
        {
            resonance = juce::jlimit (0.0f, 1.0f, resonanceNormalised);
            activeStages = juce::jlimit (1, 4, slopeChoice + 1);
            mix = juce::jlimit (0.0f, 1.0f, mixNormalised);
        }

        void processBlock (juce::AudioBuffer<float>& buffer,
                           float cutoffNormalised) noexcept
        {
            if (buffer.getNumChannels() < 2)
                return;

            const float cutoffHz = normalisedToHz (cutoffNormalised);

            if (mode == FilterMode::Comb)
            {
                processComb (buffer, cutoffHz);
                return;
            }

            updateCoefficients (cutoffHz);

            auto* left = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);
            const int numSamples = buffer.getNumSamples();

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float dryLeft = left[sample];
                const float dryRight = right[sample];

                float wetLeft = dryLeft;
                float wetRight = dryRight;

                for (int stage = 0; stage < activeStages; ++stage)
                {
                    wetLeft = biquads[0][(size_t) stage].process (wetLeft);
                    wetRight = biquads[1][(size_t) stage].process (wetRight);
                }

                mixSm.setTarget (mix);
                const float amount = mixSm.next();
                left[sample] = flushDenorm (
                    dryLeft + (wetLeft - dryLeft) * amount);
                right[sample] = flushDenorm (
                    dryRight + (wetRight - dryRight) * amount);
            }
        }

        static float normalisedToHz (float normalised) noexcept
        {
            const float value = juce::jlimit (0.0f, 1.0f, normalised);
            return 20.0f * std::pow (1000.0f, value);
        }

        static float hzToNormalised (float hz) noexcept
        {
            const float clamped = juce::jlimit (20.0f, 20000.0f, hz);
            return std::log (clamped / 20.0f) / std::log (1000.0f);
        }

    private:
        struct Biquad
        {
            float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
            float a1 = 0.0f, a2 = 0.0f;
            float z1 = 0.0f, z2 = 0.0f;

            void reset() noexcept { z1 = z2 = 0.0f; }

            void set (float newB0, float newB1, float newB2,
                      float newA0, float newA1, float newA2) noexcept
            {
                const float inverseA0 = 1.0f / juce::jmax (1.0e-9f, newA0);
                b0 = newB0 * inverseA0;
                b1 = newB1 * inverseA0;
                b2 = newB2 * inverseA0;
                a1 = newA1 * inverseA0;
                a2 = newA2 * inverseA0;
            }

            float process (float input) noexcept
            {
                const float output = b0 * input + z1;
                z1 = b1 * input - a1 * output + z2;
                z2 = b2 * input - a2 * output;
                return flushDenorm (output);
            }
        };

        void updateCoefficients (float requestedCutoff) noexcept
        {
            const float cutoff = juce::jlimit (
                20.0f, (float) sampleRate * 0.45f, requestedCutoff);
            const float q = resonanceQForStages (resonance, activeStages);
            const float omega = juce::MathConstants<float>::twoPi
                              * cutoff / (float) sampleRate;
            const float sine = std::sin (omega);
            const float cosine = std::cos (omega);
            const float alpha = sine / (2.0f * q);

            float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
            float a0 = 1.0f + alpha;
            float a1 = -2.0f * cosine;
            float a2 = 1.0f - alpha;

            switch (mode)
            {
                case FilterMode::LowPass:
                    b0 = (1.0f - cosine) * 0.5f;
                    b1 = 1.0f - cosine;
                    b2 = b0;
                    break;

                case FilterMode::HighPass:
                    b0 = (1.0f + cosine) * 0.5f;
                    b1 = -(1.0f + cosine);
                    b2 = b0;
                    break;

                case FilterMode::BandPass:
                    b0 = alpha;
                    b1 = 0.0f;
                    b2 = -alpha;
                    break;

                case FilterMode::Notch:
                    b0 = 1.0f;
                    b1 = -2.0f * cosine;
                    b2 = 1.0f;
                    break;

                case FilterMode::Peak:
                {
                    const float gainDb = peakGainDbPerStage (
                        resonance, activeStages);
                    const float amplitude = std::pow (10.0f, gainDb / 40.0f);

                    b0 = 1.0f + alpha * amplitude;
                    b1 = -2.0f * cosine;
                    b2 = 1.0f - alpha * amplitude;
                    a0 = 1.0f + alpha / amplitude;
                    a1 = -2.0f * cosine;
                    a2 = 1.0f - alpha / amplitude;
                    break;
                }

                case FilterMode::Comb:
                    break;
            }

            for (auto& channel : biquads)
                for (auto& stage : channel)
                    stage.set (b0, b1, b2, a0, a1, a2);
        }

        float readComb (int channel, float delaySamples) const noexcept
        {
            float position = (float) combWritePos - delaySamples;

            while (position < 0.0f)
                position += (float) combCapacity;

            const int first = ((int) position) % combCapacity;
            const int second = (first + 1) % combCapacity;
            const float fraction = position - std::floor (position);
            const auto& data = combBuffer[(size_t) channel];

            return data[(size_t) first]
                 + fraction * (data[(size_t) second] - data[(size_t) first]);
        }

        void processComb (juce::AudioBuffer<float>& buffer,
                          float frequencyHz) noexcept
        {
            auto* left = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);
            const int numSamples = buffer.getNumSamples();

            const float delaySamples = juce::jlimit (
                2.0f,
                (float) combCapacity - 2.0f,
                (float) sampleRate / juce::jmax (20.0f, frequencyHz));

            const float feedback = juce::jmap (
                resonance, 0.0f, 1.0f, 0.0f, 0.94f);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float dryLeft = left[sample];
                const float dryRight = right[sample];
                const float delayedLeft = readComb (0, delaySamples);
                const float delayedRight = readComb (1, delaySamples);

                const float wetLeft = dryLeft + delayedLeft * feedback;
                const float wetRight = dryRight + delayedRight * feedback;

                combBuffer[0][(size_t) combWritePos] = flushDenorm (wetLeft);
                combBuffer[1][(size_t) combWritePos] = flushDenorm (wetRight);
                combWritePos = (combWritePos + 1) % combCapacity;

                mixSm.setTarget (mix);
                const float amount = mixSm.next();
                left[sample] = flushDenorm (
                    dryLeft + (wetLeft - dryLeft) * amount);
                right[sample] = flushDenorm (
                    dryRight + (wetRight - dryRight) * amount);
            }
        }

        double sampleRate = 44100.0;
        FilterMode mode = FilterMode::LowPass;
        float resonance = 0.2f;
        float mix = 1.0f;
        int activeStages = 2;

        std::array<std::array<Biquad, 4>, 2> biquads;
        std::array<std::vector<float>, 2> combBuffer;
        int combCapacity = 11025;
        int combWritePos = 0;
        Smoothed mixSm;
    };
}
