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

            const int maxDelaySamples = (int) (2.5 * sr);
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
            delayL.reset(); delayR.reset();
            fbL = fbR = 0.0f;
            toneStateL = toneStateR = 0.0f;
        }

        void setMode (SpaceMode m) noexcept { mode = m; }

        // sizeN: 0..1 -> room size / delay time. decayN: 0..1 -> damping / feedback.
        // toneN: -1..1 tilts the wet signal darker/brighter.
        void setParams (float sizeN, float decayN, float toneN) noexcept
        {
            sizeParam = juce::jlimit (0.0f, 1.0f, sizeN);
            decayParam = juce::jlimit (0.0f, 1.0f, decayN);
            toneParam = juce::jlimit (-1.0f, 1.0f, toneN);
        }

        // mixAmount: 0..1, already modulated by caller -- this is what makes the
        // space "move" rhythmically when driven by the sequencer/LFO/envelope.
        void processBlock (juce::AudioBuffer<float>& buf, float mixAmount) noexcept
        {
            const int n = buf.getNumSamples();
            if (buf.getNumChannels() < 2) return;
            auto* L = buf.getWritePointer (0);
            auto* R = buf.getWritePointer (1);

            wetBuffer.setSize (2, n, false, false, true);
            auto* wL = wetBuffer.getWritePointer (0);
            auto* wR = wetBuffer.getWritePointer (1);

            switch (mode)
            {
                case SpaceMode::Plate:
                case SpaceMode::Hall:
                case SpaceMode::GatedReverb:
                case SpaceMode::Shimmer:
                {
                    juce::Reverb::Parameters p;
                    bool isPlate = (mode == SpaceMode::Plate);
                    bool isShimmer = (mode == SpaceMode::Shimmer);
                    p.roomSize = isPlate ? juce::jmap (sizeParam, 0.0f, 1.0f, 0.15f, 0.55f)
                                         : juce::jmap (sizeParam, 0.0f, 1.0f, 0.3f, 0.98f);
                    p.damping  = isShimmer ? juce::jmap (decayParam, 0.0f, 1.0f, 0.05f, 0.3f)
                                           : (1.0f - decayParam) * 0.9f + 0.05f;
                    p.width = 1.0f;
                    p.wetLevel = 1.0f; p.dryLevel = 0.0f; // we manage dry/wet ourselves
                    p.freezeMode = 0.0f;
                    reverb.setParameters (p);

                    for (int i = 0; i < n; ++i) { wL[i] = L[i]; wR[i] = R[i]; }
                    reverb.processStereo (wL, wR, n);

                    if (isShimmer)
                    {
                        // brightness-boosted sparkle in place of a true pitch shift
                        // (documented simplification -- see plugin notes).
                        for (int i = 0; i < n; ++i)
                        {
                            float lp = toneStateL = 0.6f * toneStateL + 0.4f * wL[i];
                            wL[i] = wL[i] + (wL[i] - lp) * 0.8f;
                            float lpR = toneStateR = 0.6f * toneStateR + 0.4f * wR[i];
                            wR[i] = wR[i] + (wR[i] - lpR) * 0.8f;
                        }
                    }

                    if (mode == SpaceMode::GatedReverb)
                    {
                        for (int i = 0; i < n; ++i)
                        {
                            float env = gateFollower.processSample (0.5f * (L[i] + R[i]));
                            float gate = env > 0.08f ? 1.0f : 0.0f;
                            gateSm = 0.995f * gateSm + 0.005f * gate;
                            wL[i] *= gateSm; wR[i] *= gateSm;
                        }
                    }
                    break;
                }

                case SpaceMode::EchoDelay:
                case SpaceMode::PanDelay:
                case SpaceMode::TapeDelay:
                {
                    float delayMs = juce::jmap (sizeParam, 0.0f, 1.0f, 40.0f, 1200.0f);
                    float delaySamples = (float) (delayMs * 0.001 * sampleRate);
                    float feedback = juce::jmap (decayParam, 0.0f, 1.0f, 0.0f, 0.92f);
                    bool tapeFlavour = (mode == SpaceMode::TapeDelay);
                    bool pingPong = (mode == SpaceMode::PanDelay);

                    delayL.setDelay (delaySamples);
                    delayR.setDelay (pingPong ? delaySamples * 1.5f : delaySamples);

                    for (int i = 0; i < n; ++i)
                    {
                        float inL = L[i] + (pingPong ? fbR : fbL);
                        float inR = R[i] + (pingPong ? fbL : fbR);

                        delayL.pushSample (0, inL);
                        delayR.pushSample (0, inR);
                        float outL = delayL.popSample (0);
                        float outR = delayR.popSample (0);

                        if (tapeFlavour)
                        {
                            outL = toneStateL = 0.75f * toneStateL + 0.25f * outL;
                            outR = toneStateR = 0.75f * toneStateR + 0.25f * outR;
                        }

                        fbL = outL * feedback;
                        fbR = outR * feedback;

                        wL[i] = outL;
                        wR[i] = outR;
                    }
                    break;
                }
            }

            // tone tilt on the wet signal, then dry/wet blend
            for (int i = 0; i < n; ++i)
            {
                mixSm.setTarget (mixAmount);
                float m = mixSm.next();

                float wl = wL[i], wr = wR[i];
                if (toneParam != 0.0f)
                {
                    tiltStateL = 0.7f * tiltStateL + 0.3f * wl;
                    tiltStateR = 0.7f * tiltStateR + 0.3f * wr;
                    wl = wl + toneParam * (wl - tiltStateL) * 0.5f;
                    wr = wr + toneParam * (wr - tiltStateR) * 0.5f;
                }

                L[i] = flushDenorm (L[i] * (1.0f - m) + wl * m);
                R[i] = flushDenorm (R[i] * (1.0f - m) + wr * m);
            }
        }

    private:
        double sampleRate = 44100.0;
        SpaceMode mode = SpaceMode::Plate;
        float sizeParam = 0.4f, decayParam = 0.4f, toneParam = 0.0f;
        float gateSm = 0.0f;
        float fbL = 0.0f, fbR = 0.0f, toneStateL = 0.0f, toneStateR = 0.0f;
        float tiltStateL = 0.0f, tiltStateR = 0.0f;

        juce::Reverb reverb;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL, delayR;
        EnvelopeFollower gateFollower;
        juce::AudioBuffer<float> wetBuffer;
        Smoothed mixSm;
    };
}
