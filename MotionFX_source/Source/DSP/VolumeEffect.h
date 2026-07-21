#pragma once
#include "DspUtils.h"

namespace mfx
{
    enum class VolumeMode { Linear, Exponential, Gate, Duck };

    class VolumeEffect
    {
    public:
        void prepare (double sr) noexcept
        {
            sampleRate = sr;
            gainSm.reset (sr, 6.0f, 1.0f);
        }
        void reset() noexcept {}

        void setMode (VolumeMode m) noexcept { mode = m; }

        // level01: 0..1 already modulated by caller, mapped internally to -24dB..+6dB
        void processBlock (juce::AudioBuffer<float>& buf, float level01) noexcept
        {
            float y = juce::jlimit (0.0f, 1.0f, level01);
            switch (mode)
            {
                case VolumeMode::Linear:      break;
                case VolumeMode::Exponential: y = y * y; break;
                case VolumeMode::Gate:        y = (y < 0.5f) ? 0.0f : 1.0f; break;
                case VolumeMode::Duck:        y = 1.0f - y; break;
            }
            float targetDb = juce::jmap (y, 0.0f, 1.0f, -24.0f, 6.0f);
            float targetGain = dbToGain (targetDb);

            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                for (int i = 0; i < buf.getNumSamples(); ++i)
                {
                    gainSm.setTarget (targetGain);
                    d[i] = flushDenorm (d[i] * gainSm.next());
                }
            }
        }

    private:
        double sampleRate = 44100.0;
        VolumeMode mode = VolumeMode::Linear;
        Smoothed gainSm;
    };
}
