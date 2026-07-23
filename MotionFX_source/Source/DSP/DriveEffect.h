#pragma once
#include "DspUtils.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
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

            pinchColour.reset();
            grooveEnvelope = 0.0f;
            spectrumWritePosition = 0;
            spectrumSkipSamples = 0;
            std::fill (
                spectrumFftData.begin(),
                spectrumFftData.end(),
                0.0f);

            for (auto& bin : spectrumUi)
                bin.store (0.0f, std::memory_order_relaxed);

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

                pinchColour.reset();
                grooveEnvelope = 0.0f;
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

        static constexpr int spectrumBinCount = 48;
        using SpectrumBins = std::array<
            std::atomic<float>, spectrumBinCount>;

        const SpectrumBins& getSpectrumBins() const noexcept
        {
            return spectrumUi;
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

            captureSpectrum (baseBlock);
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

        struct OnePoleLowPass
        {
            void reset() noexcept
            {
                state = 0.0f;
            }

            float process (float input,
                           float cutoffHz,
                           float processingRate) noexcept
            {
                const float rate = juce::jmax (1.0f, processingRate);
                const float cutoff = juce::jlimit (
                    20.0f,
                    rate * 0.45f,
                    cutoffHz);
                const float coefficient = std::exp (
                    -2.0f
                    * juce::MathConstants<float>::pi
                    * cutoff
                    / rate);

                state = coefficient * state
                      + (1.0f - coefficient) * input;
                return state;
            }

            float state = 0.0f;
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

            const auto smoothingCoefficient = [processingRate] (
                float timeMs)
            {
                return 1.0f - std::exp (
                    -1.0f
                    / (0.001f
                       * timeMs
                       * processingRate));
            };

            for (int channel = 0; channel < channels; ++channel)
            {
                driveSm[(size_t) channel].coeff =
                    smoothingCoefficient (12.0f);
                toneSm[(size_t) channel].coeff =
                    smoothingCoefficient (18.0f);
                biasSm[(size_t) channel].coeff =
                    smoothingCoefficient (18.0f);
            }

            if (mode == DriveMode::GroovePhase
                && channels >= 2)
            {
                processGroovePhaseStereo (
                    block,
                    requestedDrive,
                    processingRate);
                return;
            }

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

                    float wet = processStandardMode (
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

        void processGroovePhaseStereo (
            juce::dsp::AudioBlock<float> block,
            float requestedDrive,
            float processingRate) noexcept
        {
            auto* left = block.getChannelPointer (0);
            auto* right = block.getChannelPointer (1);
            const int samples = (int) block.getNumSamples();

            driveSm[0].setTarget (
                juce::jlimit (
                    0.0f,
                    1.0f,
                    requestedDrive));
            driveSm[1].setTarget (
                juce::jlimit (
                    0.0f,
                    1.0f,
                    requestedDrive));

            const float pinchCutoff =
                juce::jlimit (
                    80.0f,
                    processingRate * 0.45f,
                    pinchFrequencyHz
                        * std::pow (
                            2.0f,
                            pinchBandwidth + 0.70f));

            const float envelopeCoefficient = std::exp (
                -1.0f
                / juce::jmax (
                    1.0f,
                    processingRate * 0.012f));

            for (int sample = 0;
                 sample < samples;
                 ++sample)
            {
                const float inputLeft = left[sample];
                const float inputRight = right[sample];
                const float driveValue = 0.5f
                    * (driveSm[0].next()
                       + driveSm[1].next());

                const float traceControl =
                    traceEnabled
                        ? driveValue
                            * juce::jlimit (
                                0.0f,
                                2.0f,
                                traceGainDb / 12.0f)
                        : 0.0f;
                const float pinchControl =
                    pinchEnabled
                        ? driveValue
                            * juce::jlimit (
                                0.0f,
                                2.0f,
                                pinchGainDb / 12.0f)
                        : 0.0f;

                const float traceLeft = traceBand[0].process (
                    inputLeft,
                    traceFrequencyHz,
                    traceBandwidth,
                    processingRate);
                const float traceRight = traceBand[1].process (
                    inputRight,
                    traceFrequencyHz,
                    traceBandwidth,
                    processingRate);

                const float traceShapeLeft =
                    grooveRectifier (traceLeft)
                    + 0.08f
                        * (std::tanh (traceLeft * 4.0f)
                           - traceLeft);
                const float traceShapeRight =
                    grooveRectifier (traceRight)
                    + 0.08f
                        * (std::tanh (traceRight * 4.0f)
                           - traceRight);

                // Geometric playback couples the two groove walls.  Using
                // the Mid source and injecting opposite polarities recreates
                // the measured cross-channel Pinch response while leaving the
                // original stereo input intact.
                const float mid =
                    0.5f * (inputLeft + inputRight);
                const float colouredPinch =
                    pinchColour.process (
                        mid,
                        pinchCutoff,
                        processingRate);

                grooveEnvelope =
                    envelopeCoefficient * grooveEnvelope
                    + (1.0f - envelopeCoefficient)
                        * std::abs (colouredPinch);

                float pinchShape =
                    grooveRectifier (colouredPinch);

                const float oddResidual =
                    grooveCharacter == GrooveCharacter::Hard
                        ? juce::jlimit (
                              -1.0f,
                              1.0f,
                              colouredPinch * 8.0f)
                              - colouredPinch
                        : std::tanh (
                              colouredPinch * 4.0f)
                              - colouredPinch;

                pinchShape +=
                    (grooveCharacter == GrooveCharacter::Hard
                        ? 0.08f
                        : 0.02f)
                    * oddResidual;

                // A Pinch value of 12 dB at full Drive intentionally maps to
                // approximately one full-wave component for a single-channel
                // source, matching the supplied reference sweep.  The safety
                // envelope and absolute ceiling keep the useful DC punch from
                // becoming an uncontrolled speaker offset.
                const float safeComponentLimit = juce::jmin (
                    0.35f,
                    0.01f + grooveEnvelope * 4.0f);
                const float pinchComponent = juce::jlimit (
                    -safeComponentLimit,
                    safeComponentLimit,
                    pinchShape
                        * pinchControl
                        * 2.0f);

                const float traceScale =
                    grooveCharacter == GrooveCharacter::Hard
                        ? 1.30f
                        : 1.05f;
                const float traceComponentLeft = juce::jlimit (
                    -0.30f,
                    0.30f,
                    traceShapeLeft
                        * traceControl
                        * traceScale);
                const float traceComponentRight = juce::jlimit (
                    -0.30f,
                    0.30f,
                    traceShapeRight
                        * traceControl
                        * traceScale);

                float outputLeft =
                    inputLeft
                    + traceComponentLeft
                    + pinchComponent;
                float outputRight =
                    inputRight
                    + traceComponentRight
                    + (stereoPinch
                        ? -pinchComponent
                        : pinchComponent);

                if (postClip == DrivePostClip::TruePeak)
                {
                    outputLeft = truePeakCurve (outputLeft);
                    outputRight = truePeakCurve (outputRight);
                }

                left[sample] = flushDenorm (outputLeft);
                right[sample] = flushDenorm (outputRight);
            }
        }

        float grooveRectifier (float value) const noexcept
        {
            if (grooveCharacter == GrooveCharacter::Hard)
                return std::abs (value);

            constexpr float softness = 0.001f;
            return std::sqrt (
                       value * value
                       + softness * softness)
                 - softness;
        }

        void captureSpectrum (
            juce::dsp::AudioBlock<float> block) noexcept
        {
            const int channels = (int) juce::jmin (
                (size_t) 2,
                block.getNumChannels());
            const int samples = (int) block.getNumSamples();

            if (channels <= 0 || samples <= 0)
                return;

            const auto* left = block.getChannelPointer (0);
            const auto* right = channels > 1
                ? block.getChannelPointer (1)
                : left;

            for (int sample = 0;
                 sample < samples;
                 ++sample)
            {
                if (spectrumSkipSamples > 0)
                {
                    --spectrumSkipSamples;
                    continue;
                }

                spectrumFftData[(size_t) spectrumWritePosition++] =
                    0.5f * (left[sample] + right[sample]);

                if (spectrumWritePosition < spectrumFftSize)
                    continue;

                for (int index = spectrumFftSize;
                     index < spectrumFftSize * 2;
                     ++index)
                {
                    spectrumFftData[(size_t) index] = 0.0f;
                }

                spectrumWindow.multiplyWithWindowingTable (
                    spectrumFftData.data(),
                    spectrumFftSize);
                spectrumFft.performFrequencyOnlyForwardTransform (
                    spectrumFftData.data());

                const float highestFrequency = juce::jmin (
                    20000.0f,
                    (float) sampleRate * 0.45f);
                constexpr float lowestFrequency = 30.0f;

                for (int bin = 0;
                     bin < spectrumBinCount;
                     ++bin)
                {
                    const float position =
                        (float) bin
                        / (float) (spectrumBinCount - 1);
                    const float frequency =
                        lowestFrequency
                        * std::pow (
                            highestFrequency
                                / lowestFrequency,
                            position);
                    const int fftIndex = juce::jlimit (
                        1,
                        spectrumFftSize / 2,
                        (int) std::lround (
                            frequency
                            * (float) spectrumFftSize
                            / (float) sampleRate));
                    const float magnitude = juce::jmax (
                        1.0e-9f,
                        spectrumFftData[(size_t) fftIndex]
                            / (float) spectrumFftSize);
                    const float decibels = juce::Decibels
                        ::gainToDecibels (
                            magnitude,
                            -84.0f);
                    const float normalised = std::pow (
                        juce::jlimit (
                            0.0f,
                            1.0f,
                            (decibels + 84.0f) / 84.0f),
                        0.72f);
                    const float previous = spectrumUi[(size_t) bin]
                        .load (std::memory_order_relaxed);
                    spectrumUi[(size_t) bin].store (
                        juce::jmax (
                            normalised,
                            previous * 0.72f),
                        std::memory_order_relaxed);
                }

                spectrumWritePosition = 0;
                spectrumSkipSamples = spectrumFftSize * 2;
            }
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
                {
                    constexpr float threshold = 0.82f;
                    const float magnitude = std::abs (value);

                    if (magnitude <= threshold)
                        return value;

                    const float normalised =
                        (magnitude - threshold)
                        / (1.0f - threshold);
                    const float limited =
                        threshold
                        + (1.0f - threshold)
                            * std::tanh (normalised);

                    return std::copysign (
                        juce::jmin (0.999f, limited),
                        value);
                }

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
        OnePoleLowPass pinchColour;
        float grooveEnvelope = 0.0f;

        static constexpr int spectrumFftOrder = 9;
        static constexpr int spectrumFftSize =
            1 << spectrumFftOrder;
        juce::dsp::FFT spectrumFft { spectrumFftOrder };
        juce::dsp::WindowingFunction<float> spectrumWindow {
            spectrumFftSize,
            juce::dsp::WindowingFunction<float>::hann,
            true
        };
        std::array<float, spectrumFftSize * 2> spectrumFftData {};
        SpectrumBins spectrumUi {};
        int spectrumWritePosition = 0;
        int spectrumSkipSamples = 0;

        int lastEffectiveQuality = -1;
    };
}
