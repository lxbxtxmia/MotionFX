#pragma once
#include "DspUtils.h"

namespace mfx
{
    enum class WidthMode { StereoWidth, Haas, MonoBass, Phase };

    class WidthEffect
    {
    public:
        void prepare (double sr) noexcept
        {
            sampleRate = sr;
            widthSm.reset (sr, 15.0f, 1.0f);
            haasDelay.assign ((size_t) sr / 10 + 8, 0.0f); // 100ms buffer
            haasWritePos = 0;
            lowL1 = lowL2 = lowR1 = lowR2 = 0.0f;
            allpassStateR = 0.0f;
        }
        void reset() noexcept
        {
            std::fill (haasDelay.begin(), haasDelay.end(), 0.0f);
            lowL1 = lowL2 = lowR1 = lowR2 = 0.0f;
            allpassStateR = 0.0f;
        }

        void setMode (WidthMode m) noexcept { mode = m; }
        void setParams (float crossoverHzN) noexcept
        {
            // secondary knob: mono-bass crossover frequency, 80..400Hz
            crossoverHz = juce::jmap (juce::jlimit (0.0f, 1.0f, crossoverHzN), 0.0f, 1.0f, 80.0f, 400.0f);
        }

        // width01: 0..1, already modulated -> mapped to a 0..2 width multiplier (1 = unchanged)
        void processBlock (juce::AudioBuffer<float>& buf, float width01) noexcept
        {
            if (buf.getNumChannels() < 2) return;
            auto* L = buf.getWritePointer (0);
            auto* R = buf.getWritePointer (1);
            const int n = buf.getNumSamples();

            float targetMult = juce::jlimit (0.0f, 2.0f, width01 * 2.0f);
            float lpCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * crossoverHz / (float) sampleRate);

            for (int i = 0; i < n; ++i)
            {
                widthSm.setTarget (targetMult);
                float mult = widthSm.next();
                float l = L[i], r = R[i];

                if (mode == WidthMode::MonoBass)
                {
                    lowL1 = lpCoeff * lowL1 + (1.0f - lpCoeff) * l;
                    lowL2 = lpCoeff * lowL2 + (1.0f - lpCoeff) * lowL1;
                    lowR1 = lpCoeff * lowR1 + (1.0f - lpCoeff) * r;
                    lowR2 = lpCoeff * lowR2 + (1.0f - lpCoeff) * lowR1;

                    float highL = l - lowL2, highR = r - lowR2;
                    float monoLow = 0.5f * (lowL2 + lowR2);
                    float mid = 0.5f * (highL + highR);
                    float side = 0.5f * (highL - highR) * mult;
                    l = monoLow + mid + side;
                    r = monoLow + mid - side;
                }
                else
                {
                    float mid = 0.5f * (l + r);
                    float side = 0.5f * (l - r) * mult;
                    l = mid + side;
                    r = mid - side;

                    if (mode == WidthMode::Haas)
                    {
                        haasDelay[(size_t) haasWritePos] = r;
                        float delaySamples = juce::jmap (mult, 0.0f, 2.0f, 0.0f, (float) sampleRate * 0.025f);
                        float readPos = (float) haasWritePos - delaySamples;
                        int sz = (int) haasDelay.size();
                        while (readPos < 0.0f) readPos += (float) sz;
                        int i0 = (int) readPos % sz;
                        int i1 = (i0 + 1) % sz;
                        float frac = readPos - std::floor (readPos);
                        r = haasDelay[(size_t) i0] + frac * (haasDelay[(size_t) i1] - haasDelay[(size_t) i0]);
                        haasWritePos = (haasWritePos + 1) % sz;
                    }
                    else if (mode == WidthMode::Phase)
                    {
                        float a = juce::jmap (mult, 0.0f, 2.0f, -0.9f, 0.9f);
                        float y = -a * r + allpassStateR;
                        allpassStateR = r + a * y;
                        r = y;
                    }
                }

                L[i] = flushDenorm (l);
                R[i] = flushDenorm (r);
            }
        }

    private:
        double sampleRate = 44100.0;
        WidthMode mode = WidthMode::StereoWidth;
        float crossoverHz = 150.0f;
        Smoothed widthSm;

        std::vector<float> haasDelay;
        int haasWritePos = 0;

        float lowL1 = 0.0f, lowL2 = 0.0f, lowR1 = 0.0f, lowR2 = 0.0f;
        float allpassStateR = 0.0f;
    };
}
