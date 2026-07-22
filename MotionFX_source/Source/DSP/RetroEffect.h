#pragma once
#include "DspUtils.h"
#include <random>

namespace mfx
{
    enum class RetroMode { Bitcrush, Lossy, WearAndTear, Emu12Bit };

    class RetroEffect
    {
    public:
        void prepare (double sr) noexcept
        {
            sampleRate = juce::jmax (1.0, sr);
            wowLfoPhase = 0.0f;
            for (auto& sample : holdSamp)
                sample = 0.0f;
            for (auto& counter : holdCounter)
                counter = 0;
            for (auto& delay : wowDelay)
                delay.assign (2048, 0.0f);
            wowWritePos = 0;
            rng.seed (2024u);
        }

        void reset() noexcept
        {
            for (auto& sample : holdSamp)
                sample = 0.0f;
            for (auto& counter : holdCounter)
                counter = 0;
            lpStateL = lpStateR = 0.0f;
        }

        void setMode (RetroMode newMode) noexcept { mode = newMode; }

        void setParams (float rateNormalised,
                        float toneNormalised,
                        float mixNormalised) noexcept
        {
            rateParam = juce::jlimit (
                0.0f, 1.0f, rateNormalised);
            toneParam = juce::jlimit (
                -1.0f, 1.0f, toneNormalised);
            mix = juce::jlimit (
                0.0f, 1.0f, mixNormalised);
        }

        double getSampleRate() const noexcept
        {
            return sampleRate;
        }

        float bitcrushSampleRateHz (
            float rateNormalised) const noexcept
        {
            const int factor = bitcrushFactor (rateNormalised);
            return (float) sampleRate / (float) factor;
        }

        float bitcrushNormalisedFromSampleRateHz (
            float hz) const noexcept
        {
            const int factor = juce::jlimit (
                1,
                8,
                (int) std::lround (
                    sampleRate / juce::jmax (1.0f, hz)));

            return (float) (factor - 1) / 7.0f;
        }

        static float lossyBandwidthHz (
            float rateNormalised) noexcept
        {
            return juce::jmap (
                juce::jlimit (0.0f, 1.0f, rateNormalised),
                0.0f,
                1.0f,
                1200.0f,
                18000.0f);
        }

        static float lossyNormalisedFromBandwidthHz (
            float hz) noexcept
        {
            return juce::jmap (
                juce::jlimit (1200.0f, 18000.0f, hz),
                1200.0f,
                18000.0f,
                0.0f,
                1.0f);
        }

        static float wearRateHz (
            float rateNormalised) noexcept
        {
            return 0.6f
                + juce::jlimit (
                    0.0f, 1.0f, rateNormalised)
                  * 4.0f;
        }

        static float wearNormalisedFromRateHz (
            float hz) noexcept
        {
            return juce::jlimit (
                0.0f,
                1.0f,
                (hz - 0.6f) / 4.0f);
        }

        static float emuFilterHz (
            float rateNormalised) noexcept
        {
            return juce::jmap (
                juce::jlimit (0.0f, 1.0f, rateNormalised),
                0.0f,
                1.0f,
                3000.0f,
                12000.0f);
        }

        static float emuNormalisedFromFilterHz (
            float hz) noexcept
        {
            return juce::jmap (
                juce::jlimit (3000.0f, 12000.0f, hz),
                3000.0f,
                12000.0f,
                0.0f,
                1.0f);
        }

        // amount: 0..1, already modulated by caller.
        void processBlock (juce::AudioBuffer<float>& buffer,
                           float amount) noexcept
        {
            amount = juce::jlimit (0.0f, 1.0f, amount);

            for (int channel = 0;
                 channel < buffer.getNumChannels() && channel < 2;
                 ++channel)
            {
                auto* data = buffer.getWritePointer (channel);

                for (int sample = 0;
                     sample < buffer.getNumSamples();
                     ++sample)
                {
                    const float input = data[sample];
                    float wet = 0.0f;

                    switch (mode)
                    {
                        case RetroMode::Bitcrush:
                            wet = bitcrush (
                                input, channel, amount);
                            break;
                        case RetroMode::Lossy:
                            wet = lossy (
                                input, channel, amount);
                            break;
                        case RetroMode::WearAndTear:
                            wet = wearAndTear (
                                input, channel, amount);
                            break;
                        case RetroMode::Emu12Bit:
                            wet = emu12 (
                                input, channel, amount);
                            break;
                    }

                    data[sample] = flushDenorm (
                        input * (1.0f - mix)
                        + wet * mix);
                }
            }
        }

    private:
        static int bitcrushFactor (
            float rateNormalised) noexcept
        {
            return 1
                + (int) (
                    juce::jlimit (
                        0.0f,
                        1.0f,
                        rateNormalised)
                    * 7.0f);
        }

        float bitcrush (float input,
                        int channel,
                        float amount) noexcept
        {
            const float bits = juce::jmap (
                amount,
                0.0f,
                1.0f,
                16.0f,
                2.0f);
            const float levels = std::pow (2.0f, bits);
            const float quantised =
                std::round (input * levels) / levels;

            const int factor =
                bitcrushFactor (rateParam);

            if (holdCounter[(size_t) channel] <= 0)
            {
                holdSamp[(size_t) channel] = quantised;
                holdCounter[(size_t) channel] = factor;
            }

            --holdCounter[(size_t) channel];
            return holdSamp[(size_t) channel];
        }

        float lossy (float input,
                     int channel,
                     float amount) noexcept
        {
            const float selectedBandwidth =
                lossyBandwidthHz (rateParam);
            const float cutoffHz = juce::jmap (
                amount,
                0.0f,
                1.0f,
                selectedBandwidth,
                juce::jmax (
                    700.0f,
                    selectedBandwidth * 0.35f));

            const float coefficient = std::exp (
                -2.0f
                * juce::MathConstants<float>::pi
                * cutoffHz
                / (float) sampleRate);

            float& lowPassState =
                channel == 0 ? lpStateL : lpStateR;

            lowPassState = coefficient * lowPassState
                         + (1.0f - coefficient) * input;

            const float noise =
                dist (rng) * amount * 0.015f;
            return lowPassState + noise;
        }

        float wearAndTear (float input,
                           int channel,
                           float amount) noexcept
        {
            auto& delay = wowDelay[(size_t) channel];
            const int size = (int) delay.size();
            delay[(size_t) wowWritePos] = input;

            const float lfo = std::sin (wowLfoPhase);
            const float depthSamples = amount * 12.0f;
            float readPosition =
                (float) wowWritePos
                - 20.0f
                - lfo * depthSamples;

            while (readPosition < 0.0f)
                readPosition += (float) size;

            const int first = (int) readPosition % size;
            const int second = (first + 1) % size;
            const float fraction =
                readPosition - std::floor (readPosition);
            const float wowed =
                delay[(size_t) first]
                + fraction
                    * (delay[(size_t) second]
                       - delay[(size_t) first]);

            if (channel == 0)
            {
                wowLfoPhase +=
                    juce::MathConstants<float>::twoPi
                    * wearRateHz (rateParam)
                    / (float) sampleRate;

                if (wowLfoPhase
                    > juce::MathConstants<float>::twoPi)
                {
                    wowLfoPhase -=
                        juce::MathConstants<float>::twoPi;
                }

                wowWritePos = (wowWritePos + 1) % size;
            }

            float crackle = 0.0f;
            if (uniform01 (rng) < 0.0006f * amount)
                crackle = dist (rng) * 0.7f;

            const float hiss =
                dist (rng) * amount * 0.02f;

            return wowed
                     * (1.0f - amount * 0.15f)
                 + hiss
                 + crackle;
        }

        float emu12 (float input,
                     int channel,
                     float amount) noexcept
        {
            constexpr float bits = 12.0f;
            const float levels = std::pow (2.0f, bits);
            const float quantised =
                std::round (input * levels) / levels;

            const int factor =
                1 + (int) (amount * 5.0f);

            if (holdCounter[(size_t) channel] <= 0)
            {
                holdSamp[(size_t) channel] = quantised;
                holdCounter[(size_t) channel] = factor;
            }

            --holdCounter[(size_t) channel];

            const float cutoffHz =
                emuFilterHz (rateParam);
            const float coefficient = std::exp (
                -2.0f
                * juce::MathConstants<float>::pi
                * cutoffHz
                / (float) sampleRate);

            float& lowPassState =
                channel == 0 ? lpStateL : lpStateR;

            lowPassState = coefficient * lowPassState
                         + (1.0f - coefficient)
                             * holdSamp[(size_t) channel];

            return lowPassState;
        }

        double sampleRate = 44100.0;
        RetroMode mode = RetroMode::Bitcrush;
        float rateParam = 0.3f;
        float toneParam = 0.0f;
        float mix = 1.0f;

        std::array<float, 2> holdSamp {};
        std::array<int, 2> holdCounter {};
        float lpStateL = 0.0f;
        float lpStateR = 0.0f;

        std::array<std::vector<float>, 2> wowDelay;
        int wowWritePos = 0;
        float wowLfoPhase = 0.0f;

        std::mt19937 rng;
        std::uniform_real_distribution<float> dist {
            -1.0f, 1.0f
        };
        std::uniform_real_distribution<float> uniform01 {
            0.0f, 1.0f
        };
    };
}
