#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace mfx
{
    inline float flushDenorm (float v) noexcept
    {
        // Kill denormals/NaN/Inf defensively -- cheap safety net used at the
        // output of every effect stage.
        if (! std::isfinite (v)) return 0.0f;
        return (std::abs (v) < 1.0e-20f) ? 0.0f : v;
    }

    inline void sanitizeBuffer (juce::AudioBuffer<float>& buf) noexcept
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                d[i] = flushDenorm (d[i]);
        }
    }

    inline float softClip (float x, float drive) noexcept
    {
        const float g = 1.0f + drive * 9.0f;
        return std::tanh (x * g) / std::tanh (g);
    }

    inline float hardClip (float x, float drive) noexcept
    {
        const float g = 1.0f + drive * 15.0f;
        return juce::jlimit (-1.0f, 1.0f, x * g);
    }

    inline float tubeShape (float x, float drive) noexcept
    {
        const float g = 1.0f + drive * 6.0f;
        float y = x * g;
        // asymmetric transfer curve -> even harmonics, tube-ish
        float pos = std::tanh (y);
        float neg = std::tanh (y * 0.75f);
        float out = (y >= 0.0f) ? pos : neg;
        return out / std::tanh (g);
    }

    inline float overdrive (float x, float drive) noexcept
    {
        const float g = 1.0f + drive * 8.0f;
        float y = x * g;
        float out = y - (y * y * y) / 3.0f;
        out = juce::jlimit (-1.0f, 1.0f, out);
        return out / (g - (g * g * g) / 3.0f > 0.0001f ? std::tanh(g) : 1.0f);
    }

    inline float phaseDistort (float x, float drive) noexcept
    {
        // warps the waveform's phase before a soft fold -- cheap approximation
        // of phase-distortion style drive.
        float amt = drive * 0.9f;
        float warped = x + amt * std::sin (x * juce::MathConstants<float>::pi * 1.5f);
        return std::tanh (warped * (1.0f + drive * 4.0f));
    }

    inline float vinylDrive (float x, float drive, float& hpState, float coeff) noexcept
    {
        // simple asymmetric saturation plus a gentle high-pass "wow" flavour
        float sat = std::tanh (x * (1.0f + drive * 5.0f));
        hpState = coeff * hpState + (1.0f - coeff) * sat;
        float hp = sat - hpState;
        return sat * (1.0f - drive * 0.25f) + hp * (drive * 0.25f);
    }

    // One-pole smoother for click-free parameter changes.
    struct Smoothed
    {
        float value = 0.0f, target = 0.0f, coeff = 0.01f;
        void reset (double sampleRate, float timeMs, float startValue) noexcept
        {
            value = target = startValue;
            coeff = 1.0f - std::exp (-1.0f / (0.001f * timeMs * (float) sampleRate));
        }
        void setTarget (float t) noexcept { target = t; }
        float next() noexcept { value += coeff * (target - value); return value; }
    };

    // Simple one-pole low/high pass, used for tone controls & crossovers.
    struct OnePole
    {
        float z = 0.0f;
        float process (float x, float coeff) noexcept { z = coeff * z + (1.0f - coeff) * x; return z; }
    };

    inline float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }
    inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (juce::jmax (g, 1.0e-6f)); }

    // Transparent up to +-2.0 (6dB of headroom over 0dBFS); beyond that it
    // asymptotically caps around +-3.0 no matter how extreme the input gets.
    // Exists purely as a safety net against pathological stacked-feedback
    // parameter combinations -- it should never be audible in normal use.
    inline float safetyCeiling (float x) noexcept
    {
        const float T = 2.0f;
        float ax = std::abs (x);
        if (ax <= T) return x;
        float excess = ax - T;
        float compressed = T + std::tanh (excess);
        return (x < 0.0f ? -1.0f : 1.0f) * compressed;
    }
}
