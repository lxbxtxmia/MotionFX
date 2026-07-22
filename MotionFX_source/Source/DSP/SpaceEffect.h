#pragma once
#include "DspUtils.h"

namespace mfx
{
    enum class SpaceMode { Plate, Hall, EchoDelay, PanDelay, GatedReverb, TapeDelay, Shimmer };

    class SpaceEffect
    {
    public:
        void prepare (double sr, int maxBlockSize) noexcept
        {
            sampleRate = sr;
            reverb.setSampleRate (sr);
            wetBuffer.setSize (2, maxBlockSize);
            mixSm.reset (sr, 20.0f, 0.0f);

            const int maxDelaySamples = (int) (20.0 * sr);
            delayL.prepare ({ sr, (juce::uint32) maxBlockSize, 1 });
            delayR.prepare ({ sr, (juce::uint32) maxBlockSize, 1 });
            delayL.setMaximumDelayInSamples (maxDelaySamples);
            delayR.setMaximumDelayInSamples (maxDelaySamples);

            gateFollower.prepare (sr);
            gateFollower.setTimes (2.0f, 180.0f);
            reset();
        }

        void reset() noexcept
        {
            reverb.reset();
            delayL.reset();
            delayR.reset();
            fbL = fbR = 0.0f;
            toneStateL = toneStateR = 0.0f;
            tiltStateL = tiltStateR = 0.0f;
            gateSm = 0.0f;
        }

        void setMode (SpaceMode newMode) noexcept { mode = newMode; }

        void setParams (float sizeNormalised, float decayNormalised, float toneNormalised) noexcept
        {
            sizeParam = juce::jlimit (0.0f, 1.0f, sizeNormalised);
            decayParam = juce::jlimit (0.0f, 1.0f, decayNormalised);
            toneParam = juce::jlimit (-1.0f, 1.0f, toneNormalised);
        }

        void setDelayTimeSeconds (float seconds) noexcept
        {
            delayTimeSeconds = juce::jlimit (0.005f, 20.0f, seconds);
        }

        void processBlock (juce::AudioBuffer<float>& buffer, float mixAmount) noexcept
        {
            const int numSamples = buffer.getNumSamples();
            if (buffer.getNumChannels() < 2)
                return;

            auto* left = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);

            wetBuffer.setSize (2, numSamples, false, false, true);
            auto* wetLeft = wetBuffer.getWritePointer (0);
            auto* wetRight = wetBuffer.getWritePointer (1);

            switch (mode)
            {
                case SpaceMode::Plate:
                case SpaceMode::Hall:
                case SpaceMode::GatedReverb:
                case SpaceMode::Shimmer:
                {
                    juce::Reverb::Parameters parameters;
                    const bool isPlate = mode == SpaceMode::Plate;
                    const bool isShimmer = mode == SpaceMode::Shimmer;
                    const float sizeWithDecay = juce::jlimit (
                        0.0f, 1.0f, sizeParam * 0.65f + decayParam * 0.35f);

                    parameters.roomSize = isPlate
                        ? juce::jmap (sizeWithDecay, 0.0f, 1.0f, 0.15f, 0.65f)
                        : juce::jmap (sizeWithDecay, 0.0f, 1.0f, 0.3f, 0.99f);
                    parameters.damping = isShimmer
                        ? juce::jmap (decayParam, 0.0f, 1.0f, 0.05f, 0.25f)
                        : juce::jmap (toneParam, -1.0f, 1.0f, 0.9f, 0.08f);
                    parameters.width = 1.0f;
                    parameters.wetLevel = 1.0f;
                    parameters.dryLevel = 0.0f;
                    parameters.freezeMode = 0.0f;
                    reverb.setParameters (parameters);

                    for (int sample = 0; sample < numSamples; ++sample)
                    {
                        wetLeft[sample] = left[sample];
                        wetRight[sample] = right[sample];
                    }

                    reverb.processStereo (wetLeft, wetRight, numSamples);

                    if (isShimmer)
                    {
                        for (int sample = 0; sample < numSamples; ++sample)
                        {
                            const float lowLeft = toneStateL = 0.6f * toneStateL + 0.4f * wetLeft[sample];
                            const float lowRight = toneStateR = 0.6f * toneStateR + 0.4f * wetRight[sample];
                            wetLeft[sample] += (wetLeft[sample] - lowLeft) * 0.8f;
                            wetRight[sample] += (wetRight[sample] - lowRight) * 0.8f;
                        }
                    }

                    if (mode == SpaceMode::GatedReverb)
                    {
                        for (int sample = 0; sample < numSamples; ++sample)
                        {
                            const float envelope = gateFollower.processSample (
                                0.5f * (left[sample] + right[sample]));
                            const float gate = envelope > 0.08f ? 1.0f : 0.0f;
                            gateSm = 0.995f * gateSm + 0.005f * gate;
                            wetLeft[sample] *= gateSm;
                            wetRight[sample] *= gateSm;
                        }
                    }
                    break;
                }

                case SpaceMode::EchoDelay:
                case SpaceMode::PanDelay:
                case SpaceMode::TapeDelay:
                {
                    const float delaySamples = (float) juce::jlimit (
                        1.0, 20.0 * sampleRate - 2.0, delayTimeSeconds * sampleRate);
                    const float feedback = juce::jmap (decayParam, 0.0f, 1.0f, 0.0f, 0.94f);
                    const bool tapeFlavour = mode == SpaceMode::TapeDelay;
                    const bool pingPong = mode == SpaceMode::PanDelay;

                    delayL.setDelay (delaySamples);
                    delayR.setDelay (delaySamples);

                    for (int sample = 0; sample < numSamples; ++sample)
                    {
                        const float inputLeft = left[sample] + (pingPong ? fbR : fbL);
                        const float inputRight = right[sample] + (pingPong ? fbL : fbR);

                        delayL.pushSample (0, inputLeft);
                        delayR.pushSample (0, inputRight);
                        float outputLeft = delayL.popSample (0);
                        float outputRight = delayR.popSample (0);

                        if (tapeFlavour)
                        {
                            outputLeft = toneStateL = 0.75f * toneStateL + 0.25f * outputLeft;
                            outputRight = toneStateR = 0.75f * toneStateR + 0.25f * outputRight;
                        }

                        fbL = outputLeft * feedback;
                        fbR = outputRight * feedback;
                        wetLeft[sample] = outputLeft;
                        wetRight[sample] = outputRight;
                    }
                    break;
                }
            }

            for (int sample = 0; sample < numSamples; ++sample)
            {
                mixSm.setTarget (mixAmount);
                const float amount = mixSm.next();
                float processedLeft = wetLeft[sample];
                float processedRight = wetRight[sample];

                if (toneParam != 0.0f)
                {
                    tiltStateL = 0.7f * tiltStateL + 0.3f * processedLeft;
                    tiltStateR = 0.7f * tiltStateR + 0.3f * processedRight;
                    processedLeft += toneParam * (processedLeft - tiltStateL) * 0.5f;
                    processedRight += toneParam * (processedRight - tiltStateR) * 0.5f;
                }

                left[sample] = flushDenorm (left[sample] * (1.0f - amount)
                                          + processedLeft * amount);
                right[sample] = flushDenorm (right[sample] * (1.0f - amount)
                                           + processedRight * amount);
            }
        }

    private:
        double sampleRate = 44100.0;
        SpaceMode mode = SpaceMode::Plate;
        float sizeParam = 0.4f;
        float decayParam = 0.4f;
        float toneParam = 0.0f;
        float delayTimeSeconds = 0.5f;
        float gateSm = 0.0f;
        float fbL = 0.0f, fbR = 0.0f;
        float toneStateL = 0.0f, toneStateR = 0.0f;
        float tiltStateL = 0.0f, tiltStateR = 0.0f;

        juce::Reverb reverb;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL, delayR;
        EnvelopeFollower gateFollower;
        juce::AudioBuffer<float> wetBuffer;
        Smoothed mixSm;
    };
}
