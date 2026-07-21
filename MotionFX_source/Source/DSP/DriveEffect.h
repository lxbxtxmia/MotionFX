#pragma once
#include "DspUtils.h"

namespace mfx
{
    enum class DriveMode { Overdrive, Tube, SoftClip, HardClip, Vinyl, PhaseDistort };

    class DriveEffect
    {
    public:
        void prepare (double sr) noexcept
        {
            sampleRate = sr;
            for (auto& s : driveSm) s.reset (sr, 15.0f, 0.0f);
            toneLp = { 0.0f, 0.0f };
            vinylHp = { 0.0f, 0.0f };
        }
        void reset() noexcept { toneLp = { 0.0f, 0.0f }; vinylHp = { 0.0f, 0.0f }; }

        void setMode (DriveMode m) noexcept { mode = m; }
        void setParams (float toneN, float mixN, float outTrimDb) noexcept
        {
            tone = juce::jlimit (-1.0f, 1.0f, toneN);
            mix = juce::jlimit (0.0f, 1.0f, mixN);
            outGain = dbToGain (outTrimDb);
        }

        // driveAmount: 0..1, already modulated by caller
        void processBlock (juce::AudioBuffer<float>& buf, float driveAmount) noexcept
        {
            const float toneCoeff = 0.7f; // fixed tilt-filter corner
            for (int ch = 0; ch < buf.getNumChannels() && ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                for (int i = 0; i < buf.getNumSamples(); ++i)
                {
                    float drv = driveSm[(size_t) ch].next();
                    driveSm[(size_t) ch].setTarget (driveAmount);

                    float x = d[i];
                    float lp = toneLp[(size_t) ch].process (x, toneCoeff);
                    float hp = x - lp;
                    float shaped = x + tone * hp * 0.6f;

                    float wet = 0.0f;
                    switch (mode)
                    {
                        case DriveMode::Overdrive:    wet = overdrive (shaped, drv); break;
                        case DriveMode::Tube:         wet = tubeShape (shaped, drv); break;
                        case DriveMode::SoftClip:     wet = softClip (shaped, drv); break;
                        case DriveMode::HardClip:     wet = hardClip (shaped, drv); break;
                        case DriveMode::Vinyl:        wet = vinylDrive (shaped, drv, vinylHp[(size_t) ch], 0.995f); break;
                        case DriveMode::PhaseDistort: wet = phaseDistort (shaped, drv); break;
                    }
                    float out = (x * (1.0f - mix) + wet * mix) * outGain;
                    d[i] = flushDenorm (out);
                }
            }
        }

    private:
        double sampleRate = 44100.0;
        DriveMode mode = DriveMode::Tube;
        float tone = 0.0f, mix = 1.0f, outGain = 1.0f;
        std::array<Smoothed, 2> driveSm;
        std::array<OnePole, 2> toneLp;
        std::array<float, 2> vinylHp {};
    };
}
