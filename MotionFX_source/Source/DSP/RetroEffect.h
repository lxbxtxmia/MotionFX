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
            sampleRate = sr;
            wowLfoPhase = 0.0f;
            for (auto& s : holdSamp) s = 0.0f;
            for (auto& c : holdCounter) c = 0;
            for (auto& d : wowDelay) { d.assign (2048, 0.0f); }
            wowWritePos = 0;
            rng.seed (2024u);
        }
        void reset() noexcept
        {
            for (auto& s : holdSamp) s = 0.0f;
            for (auto& c : holdCounter) c = 0;
            lpStateL = lpStateR = 0.0f;
        }

        void setMode (RetroMode m) noexcept { mode = m; }
        void setParams (float rateN, float toneN, float mixN) noexcept
        {
            rateParam = juce::jlimit (0.0f, 1.0f, rateN);
            toneParam = juce::jlimit (-1.0f, 1.0f, toneN);
            mix = juce::jlimit (0.0f, 1.0f, mixN);
        }

        // amount: 0..1, already modulated by caller -- overall crush intensity
        void processBlock (juce::AudioBuffer<float>& buf, float amount) noexcept
        {
            amount = juce::jlimit (0.0f, 1.0f, amount);
            for (int ch = 0; ch < buf.getNumChannels() && ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                for (int i = 0; i < buf.getNumSamples(); ++i)
                {
                    float x = d[i];
                    float wet = 0.0f;
                    switch (mode)
                    {
                        case RetroMode::Bitcrush:    wet = bitcrush (x, ch, amount); break;
                        case RetroMode::Lossy:       wet = lossy (x, ch, amount); break;
                        case RetroMode::WearAndTear: wet = wearAndTear (x, ch, amount); break;
                        case RetroMode::Emu12Bit:    wet = emu12 (x, ch, amount); break;
                    }
                    d[i] = flushDenorm (x * (1.0f - mix) + wet * mix);
                }
            }
        }

    private:
        float bitcrush (float x, int ch, float amount) noexcept
        {
            float bits = juce::jmap (amount, 0.0f, 1.0f, 16.0f, 2.0f);
            float levels = std::pow (2.0f, bits);
            float q = std::round (x * levels) / levels;

            int factor = 1 + (int) (rateParam * 7.0f);
            if (holdCounter[(size_t) ch] <= 0)
            {
                holdSamp[(size_t) ch] = q;
                holdCounter[(size_t) ch] = factor;
            }
            --holdCounter[(size_t) ch];
            return holdSamp[(size_t) ch];
        }

        float lossy (float x, int ch, float amount) noexcept
        {
            float cutoffHz = juce::jmap (amount, 0.0f, 1.0f, 18000.0f, 1200.0f);
            float coeff = std::exp (-2.0f * juce::MathConstants<float>::pi * cutoffHz / (float) sampleRate);
            float& lp = (ch == 0) ? lpStateL : lpStateR;
            lp = coeff * lp + (1.0f - coeff) * x;

            float noise = (dist (rng)) * amount * 0.015f;
            return lp + noise;
        }

        float wearAndTear (float x, int ch, float amount) noexcept
        {
            // wow/flutter via a modulated short delay
            auto& buf = wowDelay[(size_t) ch];
            int size = (int) buf.size();
            buf[(size_t) wowWritePos] = x;

            float lfo = std::sin (wowLfoPhase);
            float depthSamples = amount * 12.0f;
            float readPos = (float) wowWritePos - 20.0f - lfo * depthSamples;
            while (readPos < 0.0f) readPos += (float) size;
            int i0 = (int) readPos % size;
            int i1 = (i0 + 1) % size;
            float frac = readPos - std::floor (readPos);
            float wowed = buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);

            if (ch == 0)
            {
                wowLfoPhase += juce::MathConstants<float>::twoPi * (0.6f + rateParam * 4.0f) / (float) sampleRate;
                if (wowLfoPhase > juce::MathConstants<float>::twoPi) wowLfoPhase -= juce::MathConstants<float>::twoPi;
                wowWritePos = (wowWritePos + 1) % size;
            }

            float crackle = 0.0f;
            if (uniform01 (rng) < (0.0006f * amount))
                crackle = (dist (rng)) * 0.7f;

            float hiss = dist (rng) * amount * 0.02f;
            return wowed * (1.0f - amount * 0.15f) + hiss + crackle;
        }

        float emu12 (float x, int ch, float amount) noexcept
        {
            const float bits = 12.0f;
            float levels = std::pow (2.0f, bits);
            float q = std::round (x * levels) / levels;

            int factor = 1 + (int) (amount * 5.0f);
            if (holdCounter[(size_t) ch] <= 0)
            {
                holdSamp[(size_t) ch] = q;
                holdCounter[(size_t) ch] = factor;
            }
            --holdCounter[(size_t) ch];

            float cutoffHz = juce::jmap (rateParam, 0.0f, 1.0f, 3000.0f, 12000.0f);
            float coeff = std::exp (-2.0f * juce::MathConstants<float>::pi * cutoffHz / (float) sampleRate);
            float& lp = (ch == 0) ? lpStateL : lpStateR;
            lp = coeff * lp + (1.0f - coeff) * holdSamp[(size_t) ch];
            return lp;
        }

        double sampleRate = 44100.0;
        RetroMode mode = RetroMode::Bitcrush;
        float rateParam = 0.3f, toneParam = 0.0f, mix = 1.0f;

        std::array<float, 2> holdSamp {};
        std::array<int, 2> holdCounter {};
        float lpStateL = 0.0f, lpStateR = 0.0f;

        std::array<std::vector<float>, 2> wowDelay;
        int wowWritePos = 0;
        float wowLfoPhase = 0.0f;

        std::mt19937 rng;
        std::uniform_real_distribution<float> dist { -1.0f, 1.0f };
        std::uniform_real_distribution<float> uniform01 { 0.0f, 1.0f };
    };
}
