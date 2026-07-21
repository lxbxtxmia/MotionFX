#pragma once
#include "DspUtils.h"

namespace mfx
{
    enum class PanMode { Linear, PingPong, Rotary };

    class PanEffect
    {
    public:
        void prepare (double sr) noexcept
        {
            sampleRate = sr;
            panSm.reset (sr, 8.0f, 0.0f);
        }
        void reset() noexcept {}

        void setMode (PanMode m) noexcept { mode = m; }
        void setParams (float widthInfluenceN) noexcept { widthInfluence = juce::jlimit (0.0f, 1.0f, widthInfluenceN); }

        // panPos: -1..1, already modulated by caller
        void processBlock (juce::AudioBuffer<float>& buf, float panPos) noexcept
        {
            if (buf.getNumChannels() < 2) return;

            float target = panPos;
            if (mode == PanMode::PingPong)
            {
                // quantise to hard L / centre / hard R for a stuttery feel
                if (target > 0.33f) target = 1.0f;
                else if (target < -0.33f) target = -1.0f;
                else target = 0.0f;
            }

            auto* L = buf.getWritePointer (0);
            auto* R = buf.getWritePointer (1);
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                panSm.setTarget (target);
                float p = panSm.next();
                float angle = (p + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                float gL = std::cos (angle);
                float gR = std::sin (angle);

                float l = L[i], r = R[i];

                if (mode == PanMode::Rotary)
                {
                    // subtle inverse width move: as pan swings, narrow the
                    // stereo image slightly to fake a circular motion.
                    float mid = (l + r) * 0.5f;
                    float side = (l - r) * 0.5f * (1.0f - widthInfluence * std::abs (p) * 0.6f);
                    l = mid + side;
                    r = mid - side;
                }

                L[i] = flushDenorm (l * gL * juce::MathConstants<float>::sqrt2);
                R[i] = flushDenorm (r * gR * juce::MathConstants<float>::sqrt2);
            }
        }

    private:
        double sampleRate = 44100.0;
        PanMode mode = PanMode::Linear;
        float widthInfluence = 0.3f;
        Smoothed panSm;
    };
}
