#pragma once
#include "DspUtils.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <memory>

namespace mfx
{
    enum class DriveMode
    {
        Overdrive = 0,
        Tube,
        SoftClip,
        HardClip,
        Tape,
        Wavefold,
        SinoidFold,
        GroovePhase
    };

    enum class DriveQuality
    {
        Eco = 0,
        Oversampled2x,
        Oversampled4x
    };

    enum class DrivePostClip
    {
        None = 0,
        Soft,
        Hard,
        TruePeak
    };

    enum class GrooveCharacter
    {
        Soft = 0,
        Hard
    };

    class DriveEffect
    {
    public:
        void prepare (double sampleRateToUse,
                      int maximumBlockSize)
        {
            sampleRate = juce::jmax (1.0, sampleRateToUse);
            maxBlockSize = juce::jmax (1, maximumBlockSize);

            oversampling2x = std::make_unique<
                juce::dsp::Oversampling<float>> (
                    2,
                    1,
                    juce::dsp::Oversampling<float>
                        ::filterHalfBandPolyphaseIIR,
                    true,
                    true);

            oversampling4x = std::make_unique<
                juce::dsp::Oversampling<float>> (
                    2,
                    2,
                    juce::dsp::Oversampling<float>
                        ::filterHalfBandPolyphaseIIR,
                    true,
                    true);

            oversampling2x->initProcessing (
                (size_t) maxBlockSize);
            oversampling4x->initProcessing (
                (size_t) maxBlockSize);

            const juce::dsp::ProcessSpec spec {
                sampleRate,
                (juce::uint32) maxBlockSize,
                2
            };

            dryWetMixer.prepare (spec);
            dryWetMixer.setMixingRule (
                juce::dsp::DryWetMixingRule::linear);

            for (int channel = 0;
                 channel < 2;
                 ++channel)
            {
                driveSm[(size_t) channel].reset (
                    sampleRate * 4.0,
                    12.0f,
                    0.0f);
                toneSm[(size_t) channel].reset (
                    sampleRate * 4.0,
                    18.0f,
                    0.0f);
                biasSm[(size_t) channel].reset (
                    sampleRate * 4.0,
                    18.0f,
                    0.0f);
                outputSm[(size_t) channel].reset (
                    sampleRate,
                    15.0f,
                    1.0f);
            }

            reset();
        }

        void reset() noexcept
        {
            if (oversampling2x != nullptr)
                oversampling2x->reset();

            if (oversampling4x != nullptr)
                oversampling4x->reset();

            dryWetMixer.reset();

            toneState = { 0.0f, 0.0f };
            tapeMemory = { 0.0f, 0.0f };
            dcInput = { 0.0f, 0.0f };
            dcOutput = { 0.0f, 0.0f };

            for (auto& state : traceBand)
                state.reset();

            for (auto& state : pinchBand)
                state.reset();

            lastEffectiveQuality = -1;
        }

        void setMode (DriveMode newMode) noexcept
        {
            if (mode != newMode)
            {
                mode = newMode;

                for (auto& state : traceBand)
                    state.reset();

                for (auto& state : pinchBand)
                    state.reset();
            }
        }

        void setParams (float toneNormalised,
                        float mixNormalised,
                        float outTrimDb,
                        float biasNormalised,
                        int qualityIndex,
                        int postClipIndex) noexcept
        {
            tone = juce::jlimit (
                -1.0f,
                1.0f,
                toneNormalised);
            mix = juce::jlimit (
                0.0f,
                1.0f,
                mixNormalised);
            outGain = dbToGain (
                juce::jlimit (
                    -24.0f,
                    24.0f,
                    outTrimDb));
            bias = juce::jlimit (
                -1.0f,
                1.0f,
                biasNormalised);
            quality = (DriveQuality) juce::jlimit (
                0,
                2,
                qualityIndex);
            postClip = (DrivePostClip) juce::jlimit (
                0,
                3,
                postClipIndex);
        }

        void setGroovePhaseParams (
            bool traceIsEnabled,
            float traceGainDbToUse,
            float traceFrequencyHzToUse,
            float traceBandwidthToUse,
            bool pinchIsEnabled,
            float pinchGainDbToUse,
            float pinchFrequencyHzToUse,
            float pinchBandwidthToUse,
            int characterIndex,
            bool stereoPinchIsEnabled) noexcept
        {
            traceEnabled = traceIsEnabled;
            traceGainDb = juce::jlimit (
                0.0f,
                24.0f,
                traceGainDbToUse);
            traceFrequencyHz = juce::jlimit (
                40.0f,
                18000.0f,
                traceFrequencyHzToUse);
            traceBandwidth = juce::jlimit (
                0.20f,
                4.0f,
                traceBandwidthToUse);

            pinchEnabled = pinchIsEnabled;
            pinchGainDb = juce::jlimit (
                0.0f,
                24.0f,
                pinchGainDbToUse);
            pinchFrequencyHz = juce::jlimit (
                40.0f,
                18000.0f,
                pinchFrequencyHzToUse);
            pinchBandwidth = juce::jlimit (
                0.20f,
                4.0f,
                pinchBandwidthToUse);

            grooveCharacter = (GrooveCharacter)
                juce::jlimit (
                    0,
                    1,
                    characterIndex);
            stereoPinch = stereoPinchIsEnabled;
        }

        int getQualityFactor() const noexcept
        {
            return factorForQuality (
                getEffectiveQualityIndex());
        }

        int getLatencySamples() const noexcept
        {
            const int effectiveQuality =
                getEffectiveQualityIndex();

            if (effectiveQuality == 1
                && oversampling2x != nullptr)
            {
                return (int) std::lround (
                    oversampling2x->getLatencyInSamples());
            }

            if (effectiveQuality == 2
                && oversampling4x != nullptr)
            {
                return (int) std::lround (
                    oversampling4x->getLatencyInSamples());
            }

            return 0;
        }

        static const char* qualityName (
            DriveQuality selectedQuality) noexcept
        {
            switch (selectedQuality)
            {
                case DriveQuality::Oversampled2x:
                    return "2x";
                case DriveQuality::Oversampled4x:
                    return "4x";
                case DriveQuality::Eco:
                default:
                    return "Eco";
            }
        }

        void processBlock (
            juce::AudioBuffer<float>& buffer,
            float driveAmount) noexcept
        {
            if (buffer.getNumChannels() < 1
                || buffer.getNumSamples() <= 0)
            {
                return;
            }

            juce::dsp::AudioBlock<float> baseBlock (buffer);
            dryWetMixer.setWetMixProportion (mix);
            dryWetMixer.setWetLatency (
                (float) getLatencySamples());
            dryWetMixer.pushDrySamples (baseBlock);

            const int effectiveQuality =
                getEffectiveQualityIndex();

            if (effectiveQuality != lastEffectiveQuality)
            {
                if (oversampling2x != nullptr)
                    oversampling2x->reset();

                if (oversampling4x != nullptr)
                    oversampling4x->reset();

                dryWetMixer.reset();
                dryWetMixer.pushDrySamples (baseBlock);
                lastEffectiveQuality = effectiveQuality;
            }

            if (effectiveQuality == 1
                && oversampling2x != nullptr)
            {
                auto upsampled =
                    oversampling2x->processSamplesUp (
                        baseBlock);
                processNonlinearBlock (
                    upsampled,
                    2,
                    driveAmount);
                oversampling2x->processSamplesDown (
                    baseBlock);
            }
            else if (effectiveQuality == 2
                     && oversampling4x != nullptr)
            {
                auto upsampled =
                    oversampling4x->processSamplesUp (
                        baseBlock);
                processNonlinearBlock (
                    upsampled,
                    4,
                    driveAmount);
                oversampling4x->processSamplesDown (
                    baseBlock);
            }
            else
            {
                processNonlinearBlock (
                    baseBlock,
                    1,
                    driveAmount);
            }

            dryWetMixer.mixWetSamples (baseBlock);

            const int channels = (int) juce::jmin (
                (size_t) 2,
                baseBlock.getNumChannels());
            const int samples = (int)
                baseBlock.getNumSamples();

            for (int channel = 0;
                 channel < channels;
                 ++channel)
            {
                auto* data =
                    baseBlock.getChannelPointer (
                        (size_t) channel);

                outputSm[(size_t) channel].setTarget (
                    outGain);

                for (int sample = 0;
                     sample < samples;
                     ++sample)
                {
                    float value =
                        data[sample]
                        * outputSm[(size_t) channel].next();

                    value = applyPostClip (
                        value,
                        postClip);

                    // True Peak additionally enforces a final sample-domain
                    // guard after the 4x inter-sample check.
                    if (postClip
                        == DrivePostClip::TruePeak)
                    {
                        value = juce::jlimit (
                            -0.999f,
                            0.999f,
                            value);
                    }

                    data[sample] = flushDenorm (value);
                }
            }
        }

    private:
        struct TptBandPass
        {
            void reset() noexcept
            {
                state1 = 0.0f;
                state2 = 0.0f;
            }

            float process (float input,
                           float frequency,
                           float bandwidth,
                           float processingRate) noexcept
            {
                const float safeRate =
                    juce::jmax (
                        1.0f,
                        processingRate);
                const float safeFrequency =
                    juce::jlimit (
                        20.0f,
                        safeRate * 0.45f,
                        frequency);

                // The user-facing B control behaves as bandwidth:
                // larger values produce a wider band.
                const float q = juce::jmap (
                    juce::jlimit (
                        0.20f,
                        4.0f,
                        bandwidth),
                    0.20f,
                    4.0f,
                    5.0f,
                    0.35f);

                const float g = std::tan (
                    juce::MathConstants<float>::pi
                    * safeFrequency
                    / safeRate);
                const float k = 1.0f
                    / juce::jmax (
                        0.05f,
                        q);
                const float a1 =
                    1.0f
                    / (1.0f + g * (g + k));
                const float a2 = g * a1;
                const float a3 = g * a2;

                const float v3 = input - state2;
                const float band =
                    a1 * state1 + a2 * v3;
                const float low =
                    state2
                    + a2 * state1
                    + a3 * v3;

                state1 = 2.0f * band - state1;
                state2 = 2.0f * low - state2;

                return band;
            }

            float state1 = 0.0f;
            float state2 = 0.0f;
        };

        static int factorForQuality (
            int qualityIndex) noexcept
        {
            if (qualityIndex == 1)
                return 2;

            if (qualityIndex == 2)
                return 4;

            return 1;
        }

        int getEffectiveQualityIndex() const noexcept
        {
            if (postClip == DrivePostClip::TruePeak)
                return 2;

            return (int) quality;
        }

        void processNonlinearBlock (
            juce::dsp::AudioBlock<float> block,
            int oversamplingFactor,
            float requestedDrive) noexcept
        {
            const int channels = (int) juce::jmin (
                (size_t) 2,
                block.getNumChannels());
            const int samples =
                (int) block.getNumSamples();
            const float processingRate =
                (float) sampleRate
                * (float) oversamplingFactor;
            const float dcCoefficient =
                std::exp (
                    -2.0f
                    * juce::MathConstants<float>::pi
                    * 8.0f
                    / processingRate);

            for (int channel = 0;
                 channel < channels;
                 ++channel)
            {
                auto* data =
                    block.getChannelPointer (
                        (size_t) channel);

                driveSm[(size_t) channel].setTarget (
                    juce::jlimit (
                        0.0f,
                        1.0f,
                        requestedDrive));
                toneSm[(size_t) channel].setTarget (
                    tone);
                biasSm[(size_t) channel].setTarget (
                    bias);

                for (int sample = 0;
                     sample < samples;
                     ++sample)
                {
                    const float input = data[sample];
                    const float driveValue =
                        driveSm[(size_t) channel].next();
                    const float toneValue =
                        toneSm[(size_t) channel].next();
                    const float biasValue =
                        biasSm[(size_t) channel].next();

                    float wet = mode
                        == DriveMode::GroovePhase
                        ? processGroovePhase (
                            input,
                            channel,
                            driveValue,
                            processingRate)
                        : processStandardMode (
                            input,
                            channel,
                            driveValue,
                            toneValue,
                            biasValue,
                            processingRate);

                    const float dcBlocked =
                        wet
                        - dcInput[(size_t) channel]
                        + dcCoefficient
                            * dcOutput[(size_t) channel];

                    dcInput[(size_t) channel] = wet;
                    dcOutput[(size_t) channel] =
                        dcBlocked;

                    wet += driveValue
                        * (dcBlocked - wet);

                    if (postClip
                        == DrivePostClip::TruePeak)
                    {
                        wet = truePeakCurve (wet);
                    }

                    data[sample] = flushDenorm (wet);
                }
            }
        }

        float processStandardMode (
            float input,
            int channel,
            float amount,
            float toneValue,
            float biasValue,
            float processingRate) noexcept
        {
            const float coefficient =
                toneCoefficient (
                    toneValue,
                    processingRate);
            auto& toneMemory =
                toneState[(size_t) channel];

            toneMemory =
                coefficient * toneMemory
                + (1.0f - coefficient)
                    * input;

            const float low = toneMemory;
            const float high = input - low;
            const float toned =
                low * (1.0f - 0.22f * toneValue)
                + high * (1.0f + 0.72f * toneValue);
            const float preShaped =
                input + amount * (toned - input);

            const float gain =
                1.0f + amount * 15.0f;
            const float biasOffset =
                biasValue * 0.38f * amount;
            const float driven =
                preShaped * gain + biasOffset;

            float nonlinear = preShaped;

            switch (mode)
            {
                case DriveMode::Overdrive:
                {
                    const float denominator =
                        juce::jmax (
                            0.001f,
                            std::atan (
                                gain * 1.25f));
                    nonlinear =
                        (std::atan (
                            driven * 1.25f)
                         - std::atan (
                             biasOffset * 1.25f))
                        / denominator;
                    break;
                }

                case DriveMode::Tube:
                {
                    const auto curve = [] (
                        float value)
                    {
                        return std::tanh (
                            value
                            * (value >= 0.0f
                                ? 1.08f
                                : 0.72f));
                    };

                    const float denominator =
                        juce::jmax (
                            0.001f,
                            std::tanh (
                                gain * 1.08f));

                    nonlinear =
                        (curve (driven)
                         - curve (biasOffset))
                        / denominator;
                    break;
                }

                case DriveMode::SoftClip:
                {
                    const float denominator =
                        juce::jmax (
                            0.001f,
                            std::tanh (gain));
                    nonlinear =
                        (std::tanh (driven)
                         - std::tanh (biasOffset))
                        / denominator;
                    break;
                }

                case DriveMode::HardClip:
                {
                    nonlinear =
                        juce::jlimit (
                            -1.0f,
                            1.0f,
                            driven)
                        - juce::jlimit (
                            -1.0f,
                            1.0f,
                            biasOffset);
                    break;
                }

                case DriveMode::Tape:
                {
                    auto& memory =
                        tapeMemory[(size_t) channel];
                    const float memoryCoefficient =
                        juce::jlimit (
                            0.001f,
                            0.16f,
                            0.035f
                            * 44100.0f
                            / processingRate);

                    memory += memoryCoefficient
                            * (driven - memory);

                    const float magnetic =
                        driven * 0.80f
                        + memory * 0.20f;
                    const float denominator =
                        juce::jmax (
                            0.001f,
                            std::tanh (
                                gain * 0.92f));

                    nonlinear =
                        (std::tanh (
                            magnetic * 0.92f)
                         - std::tanh (
                             biasOffset * 0.92f))
                        / denominator;
                    break;
                }

                case DriveMode::Wavefold:
                {
                    const auto triangleFold = [] (
                        float value)
                    {
                        return 2.0f
                            / juce::MathConstants<float>::pi
                            * std::asin (
                                std::sin (
                                    value
                                    * (juce::MathConstants<float>::pi
                                       * 0.5f)));
                    };

                    nonlinear =
                        triangleFold (driven)
                        - triangleFold (biasOffset);
                    break;
                }

                case DriveMode::SinoidFold:
                {
                    const float period =
                        juce::jmap (
                            toneValue,
                            -1.0f,
                            1.0f,
                            0.55f,
                            2.6f);

                    nonlinear =
                        std::sin (
                            driven
                            * period
                            * juce::MathConstants<float>::pi)
                        - std::sin (
                            biasOffset
                            * period
                            * juce::MathConstants<float>::pi);
                    break;
                }

                case DriveMode::GroovePhase:
                    break;
            }

            nonlinear = juce::jlimit (
                -2.5f,
                2.5f,
                nonlinear);

            return preShaped
                + amount
                    * (nonlinear - preShaped);
        }

        float processGroovePhase (
            float input,
            int channel,
            float amount,
            float processingRate) noexcept
        {
            const float trace =
                traceBand[(size_t) channel].process (
                    input,
                    traceFrequencyHz,
                    traceBandwidth,
                    processingRate);
            const float pinch =
                pinchBand[(size_t) channel].process (
                    input,
                    pinchFrequencyHz,
                    pinchBandwidth,
                    processingRate);

            const float traceAmount =
                traceEnabled
                    ? amount
                        * (traceGainDb / 24.0f)
                    : 0.0f;
            const float pinchAmount =
                pinchEnabled
                    ? amount
                        * (pinchGainDb / 24.0f)
                    : 0.0f;

            float evenHarmonics = 0.0f;
            float oddHarmonics = 0.0f;

            if (grooveCharacter
                == GrooveCharacter::Soft)
            {
                // Full-wave-like tracing produces a smooth even-harmonic
                // component; the DC blocker removes the static offset.
                evenHarmonics =
                    std::sqrt (
                        trace * trace + 0.0025f)
                    - 0.05f;

                const float pinchDriven =
                    pinch * (2.0f + 5.0f * amount);
                oddHarmonics =
                    std::tanh (pinchDriven)
                    - pinch;
            }
            else
            {
                evenHarmonics =
                    std::abs (trace);

                const float pinchDriven =
                    pinch * (3.0f + 9.0f * amount);
                oddHarmonics =
                    juce::jlimit (
                        -1.0f,
                        1.0f,
                        pinchDriven)
                    - pinch;
            }

            float pinchPolarity = 1.0f;

            if (stereoPinch
                && channel == 1)
            {
                // The Pinch component is deliberately 180 degrees out of
                // phase on the right channel to enrich the stereo image.
                pinchPolarity = -1.0f;
            }

            const float modelled =
                input
                + evenHarmonics
                    * traceAmount
                    * 1.35f
                + oddHarmonics
                    * pinchAmount
                    * pinchPolarity
                    * 0.85f;

            return juce::jlimit (
                -3.0f,
                3.0f,
                modelled);
        }

        static float toneCoefficient (
            float toneValue,
            float processingRate) noexcept
        {
            const float normalised =
                juce::jlimit (
                    0.0f,
                    1.0f,
                    (toneValue + 1.0f) * 0.5f);
            const float cutoff =
                650.0f
                * std::pow (
                    18000.0f / 650.0f,
                    normalised);

            return std::exp (
                -2.0f
                * juce::MathConstants<float>::pi
                * cutoff
                / juce::jmax (
                    1.0f,
                    processingRate));
        }

        static float truePeakCurve (
            float value) noexcept
        {
            constexpr float ceiling = 0.985f;
            const float absolute =
                std::abs (value);

            if (absolute <= ceiling)
                return value;

            const float excess =
                absolute - ceiling;
            const float limited =
                ceiling
                + (1.0f - ceiling)
                    * std::tanh (
                        excess
                        / (1.0f - ceiling));

            return std::copysign (
                juce::jmin (
                    0.999f,
                    limited),
                value);
        }

        static float applyPostClip (
            float value,
            DrivePostClip clipMode) noexcept
        {
            switch (clipMode)
            {
                case DrivePostClip::Soft:
                    return juce::jlimit (
                        -1.0f,
                        1.0f,
                        std::tanh (value * 1.6f)
                            / std::tanh (1.6f));

                case DrivePostClip::Hard:
                    return juce::jlimit (
                        -1.0f,
                        1.0f,
                        value);

                case DrivePostClip::TruePeak:
                    return truePeakCurve (value);

                case DrivePostClip::None:
                default:
                    return value;
            }
        }

        double sampleRate = 44100.0;
        int maxBlockSize = 512;

        DriveMode mode = DriveMode::Tube;
        DriveQuality quality =
            DriveQuality::Oversampled2x;
        DrivePostClip postClip =
            DrivePostClip::None;

        float tone = 0.0f;
        float mix = 1.0f;
        float outGain = 1.0f;
        float bias = 0.0f;

        bool traceEnabled = true;
        float traceGainDb = 6.0f;
        float traceFrequencyHz = 680.0f;
        float traceBandwidth = 0.32f;

        bool pinchEnabled = true;
        float pinchGainDb = 6.0f;
        float pinchFrequencyHz = 7500.0f;
        float pinchBandwidth = 3.0f;
        GrooveCharacter grooveCharacter =
            GrooveCharacter::Soft;
        bool stereoPinch = true;

        std::unique_ptr<
            juce::dsp::Oversampling<float>>
            oversampling2x;
        std::unique_ptr<
            juce::dsp::Oversampling<float>>
            oversampling4x;
        juce::dsp::DryWetMixer<float>
            dryWetMixer { 512 };

        std::array<Smoothed, 2> driveSm;
        std::array<Smoothed, 2> toneSm;
        std::array<Smoothed, 2> biasSm;
        std::array<Smoothed, 2> outputSm;

        std::array<float, 2> toneState {};
        std::array<float, 2> tapeMemory {};
        std::array<float, 2> dcInput {};
        std::array<float, 2> dcOutput {};

        std::array<TptBandPass, 2> traceBand;
        std::array<TptBandPass, 2> pinchBand;

        int lastEffectiveQuality = -1;
    };
}
