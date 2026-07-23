#pragma once
#include "DspUtils.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

namespace mfx
{
    enum class RetroMode
    {
        Bitcrush = 0,
        Lossy,
        WearAndTear,
        Sp12Bit,
        Tape,
        VinylDust
    };

    enum class BitHoldMode { Step = 0, Linear, Smooth };
    enum class LossyQuality { Eco = 0, Normal, High };
    enum class SpFilterMode { Unfiltered = 0, Static, Dynamic };
    enum class TapeMachine { ReelToReel = 0, Cassette };
    enum class TapeSpeed { Ips1_875 = 0, Ips3_75, Ips7_5, Ips15, Ips30 };
    enum class TapeNoiseReduction { Off = 0, BStyle, CStyle };

    class RetroEffect
    {
    public:
        static constexpr int spectrumBands = 48;
        static constexpr float spBaseSampleRate = 26040.0f;

        void prepare (double sampleRateToUse, int maximumBlockSize)
        {
            sampleRate = juce::jmax (1.0, sampleRateToUse);
            maxBlockSize = juce::jmax (1, maximumBlockSize);

            const juce::dsp::ProcessSpec mixSpec {
                sampleRate,
                (juce::uint32) maxBlockSize,
                2
            };
            dryWetMixer.prepare (mixSpec);
            dryWetMixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);

            lossyProcessors[0] = std::make_unique<SpectralLossy> (8, *this);
            lossyProcessors[1] = std::make_unique<SpectralLossy> (9, *this);
            lossyProcessors[2] = std::make_unique<SpectralLossy> (10, *this);
            for (auto& processor : lossyProcessors)
                processor->prepare (sampleRate);

            const int delaySize = juce::jmax (
                8192,
                (int) std::ceil (sampleRate * 0.12));
            for (auto& delay : motionDelay)
                delay.assign ((size_t) delaySize, 0.0f);

            reset();
        }

        void reset() noexcept
        {
            dryWetMixer.reset();
            for (auto& processor : lossyProcessors)
                if (processor != nullptr)
                    processor->reset();

            for (auto& state : bitState)
                state = {};
            for (auto& state : spState)
                state = {};
            for (auto& delay : motionDelay)
                std::fill (delay.begin(), delay.end(), 0.0f);

            motionWritePosition = 0;
            wowPhase = flutterPhase = 0.0f;
            wearDropoutGain = 1.0f;
            wearDropoutTarget = 1.0f;
            wearDropoutSamples = 0;
            tapeDropoutGain = 1.0f;
            tapeDropoutTarget = 1.0f;
            tapeDropoutSamples = 0;

            wearLowpass = {};
            tapeLowpass = {};
            tapeHeadBumpLow = {};
            tapeHeadBumpBand = {};
            tapeMemory = {};
            tapeNoiseLow = {};
            tapeNrEncodeLow = {};
            tapeNrDecodeLow = {};
            tapeNrEnvelope = {};
            tapeDenoiseEnvelope = {};
            vinylLowpass = {};
            vinylHighpassLow = {};
            vinylCrackleEnvelope = {};
            spInputLowpass = {};
            spEnvelope = {};
            bitAntiAliasLowpass = {};

            random.seed (0x4d465831u);
            lastMode = -1;
            lastLossyQuality = -1;

            uiMotion.store (0.0f, std::memory_order_relaxed);
            uiDropout.store (0.0f, std::memory_order_relaxed);
            for (auto& value : uiInputSpectrum)
                value.store (0.0f, std::memory_order_relaxed);
            for (auto& value : uiOutputSpectrum)
                value.store (0.0f, std::memory_order_relaxed);
        }

        void setMode (RetroMode newMode) noexcept
        {
            mode = newMode;
        }

        void setMix (float newMix) noexcept
        {
            mix = juce::jlimit (0.0f, 1.0f, newMix);
        }

        void setBitcrushParams (int bitsToUse,
                                float sampleRateHzToUse,
                                int holdModeIndex,
                                bool useDither,
                                bool useAntiAlias) noexcept
        {
            bitDepth = juce::jlimit (2, 16, bitsToUse);
            bitSampleRateHz = juce::jlimit (
                500.0f,
                96000.0f,
                sampleRateHzToUse);
            bitHoldMode = (BitHoldMode) juce::jlimit (0, 2, holdModeIndex);
            bitDither = useDither;
            bitAntiAlias = useAntiAlias;
        }

        void setLossyParams (float bandwidthHzToUse,
                             float detailToUse,
                             float damageToUse,
                             int qualityIndex,
                             bool stereoLinkToUse) noexcept
        {
            lossyBandwidthHz = juce::jlimit (
                500.0f,
                24000.0f,
                bandwidthHzToUse);
            lossyDetail = juce::jlimit (0.0f, 1.0f, detailToUse);
            lossyDamage = juce::jlimit (0.0f, 1.0f, damageToUse);
            lossyQuality = (LossyQuality) juce::jlimit (0, 2, qualityIndex);
            lossyStereoLink = stereoLinkToUse;
        }

        void setWearParams (float wowToUse,
                            float flutterToUse,
                            float dropoutToUse,
                            float ageToUse,
                            float stereoDriftToUse) noexcept
        {
            wearWow = juce::jlimit (0.0f, 1.0f, wowToUse);
            wearFlutter = juce::jlimit (0.0f, 1.0f, flutterToUse);
            wearDropout = juce::jlimit (0.0f, 1.0f, dropoutToUse);
            wearAge = juce::jlimit (0.0f, 1.0f, ageToUse);
            wearStereoDrift = juce::jlimit (0.0f, 1.0f, stereoDriftToUse);
        }

        void setSpParams (int clockSemitonesToUse,
                          int filterModeIndex,
                          float filterCutoffHzToUse,
                          float inputDriveToUse) noexcept
        {
            spClockSemitones = juce::jlimit (-12, 12, clockSemitonesToUse);
            spFilterMode = (SpFilterMode) juce::jlimit (0, 2, filterModeIndex);
            spFilterCutoffHz = juce::jlimit (
                1200.0f,
                14000.0f,
                filterCutoffHzToUse);
            spInputDrive = juce::jlimit (0.0f, 1.0f, inputDriveToUse);
        }

        void setTapeParams (int machineIndex,
                            int speedIndex,
                            float driveToUse,
                            float ageToUse,
                            float motionToUse,
                            float noiseToUse,
                            int noiseReductionIndex,
                            float noiseReductionAmountToUse,
                            float denoiseToUse) noexcept
        {
            tapeMachine = (TapeMachine) juce::jlimit (0, 1, machineIndex);
            tapeSpeed = (TapeSpeed) juce::jlimit (0, 4, speedIndex);
            tapeDrive = juce::jlimit (0.0f, 1.0f, driveToUse);
            tapeAge = juce::jlimit (0.0f, 1.0f, ageToUse);
            tapeMotion = juce::jlimit (0.0f, 1.0f, motionToUse);
            tapeNoise = juce::jlimit (0.0f, 1.0f, noiseToUse);
            tapeNoiseReduction = (TapeNoiseReduction) juce::jlimit (
                0,
                2,
                noiseReductionIndex);
            tapeNoiseReductionAmount = juce::jlimit (
                0.0f,
                1.0f,
                noiseReductionAmountToUse);
            tapeDenoise = juce::jlimit (0.0f, 1.0f, denoiseToUse);
        }

        void setVinylParams (float dustToUse,
                             float crackleToUse,
                             float surfaceToUse,
                             float wearToUse) noexcept
        {
            vinylDust = juce::jlimit (0.0f, 1.0f, dustToUse);
            vinylCrackle = juce::jlimit (0.0f, 1.0f, crackleToUse);
            vinylSurface = juce::jlimit (0.0f, 1.0f, surfaceToUse);
            vinylWear = juce::jlimit (0.0f, 1.0f, wearToUse);
        }

        int getLatencySamples() const noexcept
        {
            if (mode != RetroMode::Lossy)
                return 0;

            const int index = juce::jlimit (0, 2, (int) lossyQuality);
            const auto& processor = lossyProcessors[(size_t) index];
            return processor != nullptr ? processor->getLatencySamples() : 0;
        }

        double getSampleRate() const noexcept { return sampleRate; }

        static float tapeSpeedIps (TapeSpeed speed) noexcept
        {
            switch (speed)
            {
                case TapeSpeed::Ips1_875: return 1.875f;
                case TapeSpeed::Ips3_75: return 3.75f;
                case TapeSpeed::Ips7_5: return 7.5f;
                case TapeSpeed::Ips15: return 15.0f;
                case TapeSpeed::Ips30: return 30.0f;
            }
            return 15.0f;
        }

        float spEffectiveSampleRate() const noexcept
        {
            return juce::jmin (
                (float) sampleRate,
                spBaseSampleRate
                    * std::pow (2.0f, (float) spClockSemitones / 12.0f));
        }

        void processBlock (juce::AudioBuffer<float>& buffer,
                           float amount) noexcept
        {
            const int channels = juce::jmin (2, buffer.getNumChannels());
            const int samples = buffer.getNumSamples();
            if (channels <= 0 || samples <= 0)
                return;

            amount = juce::jlimit (0.0f, 1.0f, amount);
            const int modeIndex = (int) mode;
            const int qualityIndex = (int) lossyQuality;

            if (lastMode != modeIndex || lastLossyQuality != qualityIndex)
            {
                dryWetMixer.reset();

                if (lastMode != modeIndex)
                {
                    for (auto& delay : motionDelay)
                        std::fill (delay.begin(), delay.end(), 0.0f);
                    motionWritePosition = 0;
                    wowPhase = flutterPhase = 0.0f;
                    bitState = {};
                    spState = {};
                    wearDropoutGain = wearDropoutTarget = 1.0f;
                    wearDropoutSamples = 0;
                    tapeMemory = {};
                    vinylCrackleEnvelope = {};
                }

                if (mode == RetroMode::Lossy)
                {
                    const auto& selected = lossyProcessors[(size_t) qualityIndex];
                    if (selected != nullptr)
                        selected->reset();
                }
                lastMode = modeIndex;
                lastLossyQuality = qualityIndex;
            }

            juce::dsp::AudioBlock<float> block (buffer);
            dryWetMixer.setWetLatency ((float) getLatencySamples());
            dryWetMixer.setWetMixProportion (mix);
            dryWetMixer.pushDrySamples (block);

            switch (mode)
            {
                case RetroMode::Bitcrush:
                    processBitcrush (buffer, amount);
                    break;
                case RetroMode::Lossy:
                    processLossy (buffer, amount);
                    break;
                case RetroMode::WearAndTear:
                    processWear (buffer, amount);
                    break;
                case RetroMode::Sp12Bit:
                    processSp12 (buffer, amount);
                    break;
                case RetroMode::Tape:
                    processTape (buffer, amount);
                    break;
                case RetroMode::VinylDust:
                    processVinyl (buffer, amount);
                    break;
            }

            dryWetMixer.mixWetSamples (block);
            sanitizeBuffer (buffer);
        }

        std::array<std::atomic<float>, spectrumBands> uiInputSpectrum {};
        std::array<std::atomic<float>, spectrumBands> uiOutputSpectrum {};
        std::atomic<float> uiMotion { 0.0f };
        std::atomic<float> uiDropout { 0.0f };

    private:
        struct BitState
        {
            double phase = 1.0;
            float previous = 0.0f;
            float held = 0.0f;
            float smooth = 0.0f;
        };

        struct SpState
        {
            double phase = 1.0;
            float held = 0.0f;
            std::array<float, 4> filter {};
        };

        struct SpectralLossy
        {
            SpectralLossy (int orderToUse, RetroEffect& ownerToUse)
                : order (orderToUse),
                  fftSize (1 << orderToUse),
                  hopSize (fftSize / 4),
                  fft (orderToUse),
                  owner (ownerToUse)
            {
            }

            void prepare (double sampleRateToUse)
            {
                sampleRate = sampleRateToUse;
                const int outputSize = fftSize * 4;
                for (int channel = 0; channel < 2; ++channel)
                {
                    inputRing[(size_t) channel].assign ((size_t) fftSize, 0.0f);
                    outputRing[(size_t) channel].assign ((size_t) outputSize, 0.0f);
                    fftData[(size_t) channel].assign ((size_t) fftSize * 2, 0.0f);
                }
                window.resize ((size_t) fftSize);
                for (int index = 0; index < fftSize; ++index)
                {
                    const float phase = juce::MathConstants<float>::twoPi
                        * (float) index / (float) fftSize;
                    window[(size_t) index] = std::sqrt (0.5f - 0.5f * std::cos (phase));
                }
                reset();
            }

            void reset() noexcept
            {
                for (auto& ring : inputRing)
                    std::fill (ring.begin(), ring.end(), 0.0f);
                for (auto& ring : outputRing)
                    std::fill (ring.begin(), ring.end(), 0.0f);
                inputWrite = outputRead = 0;
                samplesUntilFrame = fftSize;
                frameCounter = 0;
            }

            int getLatencySamples() const noexcept { return fftSize; }

            void process (juce::AudioBuffer<float>& buffer,
                          float amount,
                          float bandwidth,
                          float detail,
                          float damage,
                          bool stereoLink) noexcept
            {
                const int channels = juce::jmin (2, buffer.getNumChannels());
                const int samples = buffer.getNumSamples();
                const int outputSize = fftSize * 4;

                for (int sample = 0; sample < samples; ++sample)
                {
                    for (int channel = 0; channel < channels; ++channel)
                    {
                        auto* data = buffer.getWritePointer (channel);
                        inputRing[(size_t) channel][(size_t) inputWrite] = data[sample];
                        data[sample] = outputRing[(size_t) channel][(size_t) outputRead];
                        outputRing[(size_t) channel][(size_t) outputRead] = 0.0f;
                    }

                    inputWrite = (inputWrite + 1) % fftSize;
                    outputRead = (outputRead + 1) % outputSize;

                    if (--samplesUntilFrame <= 0)
                    {
                        samplesUntilFrame = hopSize;
                        processFrame (channels, amount, bandwidth, detail, damage, stereoLink);
                        ++frameCounter;
                    }
                }
            }

        private:
            static std::uint32_t hashValue (std::uint32_t value) noexcept
            {
                value ^= value >> 16;
                value *= 0x7feb352du;
                value ^= value >> 15;
                value *= 0x846ca68bu;
                value ^= value >> 16;
                return value;
            }

            void processFrame (int channels,
                               float amount,
                               float bandwidth,
                               float detail,
                               float damage,
                               bool stereoLink) noexcept
            {
                std::array<float, spectrumBands> inputDisplay {};
                std::array<float, spectrumBands> outputDisplay {};
                const int bins = fftSize / 2;
                const float effectiveBandwidth = juce::jmap (
                    amount,
                    juce::jmin (bandwidth, (float) sampleRate * 0.48f),
                    juce::jmax (700.0f, bandwidth * 0.32f));
                const float damageAmount = amount * damage;
                const float detailAmount = juce::jlimit (
                    0.0f,
                    1.0f,
                    1.0f - amount * (1.0f - detail));
                const float magnitudeSteps = juce::jmap (
                    detailAmount,
                    8.0f,
                    512.0f);

                for (int channel = 0; channel < channels; ++channel)
                {
                    auto& data = fftData[(size_t) channel];
                    std::fill (data.begin(), data.end(), 0.0f);

                    for (int index = 0; index < fftSize; ++index)
                    {
                        const int ringIndex = (inputWrite + index) % fftSize;
                        data[(size_t) index] = inputRing[(size_t) channel][(size_t) ringIndex]
                            * window[(size_t) index];
                    }

                    fft.performRealOnlyForwardTransform (data.data(), true);

                    for (int bin = 0; bin <= bins; ++bin)
                    {
                        const bool dcBin = bin == 0;
                        const bool nyquistBin = bin == bins;
                        const int realIndex = dcBin ? 0 : (nyquistBin ? 1 : bin * 2);
                        const int imaginaryIndex = bin * 2 + 1;
                        const float real = data[(size_t) realIndex];
                        const float imaginary = (dcBin || nyquistBin)
                            ? 0.0f
                            : data[(size_t) imaginaryIndex];
                        float magnitude = std::sqrt (real * real + imaginary * imaginary);
                        float phase = std::atan2 (imaginary, real);
                        const float frequency = (float) bin * (float) sampleRate / (float) fftSize;

                        const int displayBand = juce::jlimit (
                            0,
                            spectrumBands - 1,
                            (int) std::floor (
                                std::log2 (juce::jmax (20.0f, frequency) / 20.0f)
                                / std::log2 (24000.0f / 20.0f)
                                * (float) spectrumBands));
                        inputDisplay[(size_t) displayBand] = juce::jmax (
                            inputDisplay[(size_t) displayBand],
                            magnitude);

                        float spectralGain = 1.0f;
                        if (frequency > effectiveBandwidth)
                        {
                            const float octaveDistance = std::log2 (
                                frequency / juce::jmax (20.0f, effectiveBandwidth));
                            spectralGain *= std::exp (-octaveDistance * (2.0f + 6.0f * amount));
                        }

                        const std::uint32_t linkChannel = stereoLink ? 0u : (std::uint32_t) channel;
                        const std::uint32_t noise = hashValue (
                            (std::uint32_t) frameCounter * 1315423911u
                            ^ (std::uint32_t) bin * 2654435761u
                            ^ linkChannel * 2246822519u);
                        const float random01 = (float) (noise & 0x00ffffffu) / 16777215.0f;
                        const float dropoutProbability = damageAmount * damageAmount * 0.38f;
                        if (random01 < dropoutProbability && bin > 2)
                            spectralGain *= juce::jmap (damageAmount, 0.32f, 0.0f);

                        magnitude *= spectralGain;
                        if (magnitude > 1.0e-12f)
                        {
                            const float logMagnitude = std::log2 (magnitude + 1.0e-12f);
                            magnitude = std::pow (
                                2.0f,
                                std::round (logMagnitude * magnitudeSteps) / magnitudeSteps);
                        }

                        const float phaseSteps = juce::jmap (
                            detailAmount,
                            8.0f,
                            256.0f);
                        phase = std::round (
                            phase / juce::MathConstants<float>::twoPi * phaseSteps)
                            / phaseSteps * juce::MathConstants<float>::twoPi;

                        data[(size_t) realIndex] = magnitude * std::cos (phase);
                        if (! dcBin && ! nyquistBin)
                            data[(size_t) imaginaryIndex] = magnitude * std::sin (phase);
                        outputDisplay[(size_t) displayBand] = juce::jmax (
                            outputDisplay[(size_t) displayBand],
                            magnitude);
                    }

                    fft.performRealOnlyInverseTransform (data.data());

                    const float overlapNormalisation = 0.5f / (float) fftSize;
                    const int outputSize = fftSize * 4;
                    for (int index = 0; index < fftSize; ++index)
                    {
                        const int target = (outputRead + index) % outputSize;
                        outputRing[(size_t) channel][(size_t) target] +=
                            data[(size_t) index]
                            * window[(size_t) index]
                            * overlapNormalisation;
                    }
                }

                float inputPeak = 1.0e-9f;
                float outputPeak = 1.0e-9f;
                for (int band = 0; band < spectrumBands; ++band)
                {
                    inputPeak = juce::jmax (inputPeak, inputDisplay[(size_t) band]);
                    outputPeak = juce::jmax (outputPeak, outputDisplay[(size_t) band]);
                }
                for (int band = 0; band < spectrumBands; ++band)
                {
                    owner.uiInputSpectrum[(size_t) band].store (
                        juce::jlimit (0.0f, 1.0f, inputDisplay[(size_t) band] / inputPeak),
                        std::memory_order_relaxed);
                    owner.uiOutputSpectrum[(size_t) band].store (
                        juce::jlimit (0.0f, 1.0f, outputDisplay[(size_t) band] / inputPeak),
                        std::memory_order_relaxed);
                }
            }

            int order = 9;
            int fftSize = 512;
            int hopSize = 128;
            double sampleRate = 44100.0;
            juce::dsp::FFT fft;
            RetroEffect& owner;
            std::array<std::vector<float>, 2> inputRing;
            std::array<std::vector<float>, 2> outputRing;
            std::array<std::vector<float>, 2> fftData;
            std::vector<float> window;
            int inputWrite = 0;
            int outputRead = 0;
            int samplesUntilFrame = 512;
            std::uint64_t frameCounter = 0;
        };

        static float onePoleCoefficient (float cutoff, float rate) noexcept
        {
            return std::exp (
                -2.0f * juce::MathConstants<float>::pi
                * juce::jlimit (10.0f, rate * 0.45f, cutoff)
                / juce::jmax (1.0f, rate));
        }

        float randomBipolar() noexcept
        {
            return randomDistribution (random);
        }

        float randomUnipolar() noexcept
        {
            return 0.5f + 0.5f * randomBipolar();
        }

        float quantise (float input, float bits, bool dither) noexcept
        {
            const float clampedBits = juce::jlimit (2.0f, 24.0f, bits);
            const float levels = std::pow (2.0f, clampedBits - 1.0f);
            float value = input;
            if (dither)
                value += (randomBipolar() + randomBipolar()) * 0.5f / levels;
            return std::round (value * levels) / levels;
        }

        float readMotionDelay (int channel, float delaySamples) const noexcept
        {
            const auto& delay = motionDelay[(size_t) channel];
            if (delay.empty())
                return 0.0f;
            const int size = (int) delay.size();
            float position = (float) motionWritePosition - delaySamples;
            while (position < 0.0f)
                position += (float) size;
            while (position >= (float) size)
                position -= (float) size;
            const int first = (int) position;
            const int second = (first + 1) % size;
            const float fraction = position - (float) first;
            return delay[(size_t) first]
                + fraction * (delay[(size_t) second] - delay[(size_t) first]);
        }

        void processBitcrush (juce::AudioBuffer<float>& buffer, float amount) noexcept
        {
            const float hostRate = (float) sampleRate;
            const float selectedRate = juce::jmin (hostRate, bitSampleRateHz);
            const float effectiveRate = juce::jmap (amount, hostRate, selectedRate);
            const float effectiveBits = juce::jmap (amount, 24.0f, (float) bitDepth);
            const float phaseIncrement = effectiveRate / hostRate;
            const float antiAliasCutoff = juce::jmax (200.0f, effectiveRate * 0.45f);
            const float antiAliasCoefficient = onePoleCoefficient (antiAliasCutoff, hostRate);

            for (int channel = 0; channel < juce::jmin (2, buffer.getNumChannels()); ++channel)
            {
                auto* data = buffer.getWritePointer (channel);
                auto& state = bitState[(size_t) channel];
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    float input = data[sample];
                    if (bitAntiAlias)
                        input = bitAntiAliasLowpass[(size_t) channel].process (input, antiAliasCoefficient);

                    state.phase += phaseIncrement;
                    if (state.phase >= 1.0)
                    {
                        state.phase -= std::floor (state.phase);
                        state.previous = state.held;
                        state.held = quantise (input, effectiveBits, bitDither && amount > 0.001f);
                    }

                    float output = state.held;
                    if (bitHoldMode == BitHoldMode::Linear)
                        output = state.previous + (state.held - state.previous) * (float) state.phase;
                    else if (bitHoldMode == BitHoldMode::Smooth)
                    {
                        const float smoothing = juce::jlimit (0.02f, 1.0f, phaseIncrement * 2.5f);
                        state.smooth += smoothing * (state.held - state.smooth);
                        output = state.smooth;
                    }
                    data[sample] = flushDenorm (output);
                }
            }
        }

        void processLossy (juce::AudioBuffer<float>& buffer, float amount) noexcept
        {
            const int index = juce::jlimit (0, 2, (int) lossyQuality);
            auto& processor = lossyProcessors[(size_t) index];
            if (processor != nullptr)
                processor->process (
                    buffer,
                    amount,
                    lossyBandwidthHz,
                    lossyDetail,
                    lossyDamage,
                    lossyStereoLink);
        }

        void processWear (juce::AudioBuffer<float>& buffer, float amount) noexcept
        {
            const int samples = buffer.getNumSamples();
            const int delaySize = motionDelay[0].empty() ? 1 : (int) motionDelay[0].size();
            const float wowRate = 0.12f + 0.95f * wearWow;
            const float flutterRate = 4.0f + 11.0f * wearFlutter;
            const float wowDepth = amount * wearWow * (3.0f + 18.0f * wearAge);
            const float flutterDepth = amount * wearFlutter * (0.5f + 4.0f * wearAge);
            const float cutoff = juce::jmap (amount * wearAge, 18000.0f, 3200.0f);
            const float cutoffCoefficient = onePoleCoefficient (cutoff, (float) sampleRate);

            for (int sample = 0; sample < samples; ++sample)
            {
                for (int channel = 0; channel < 2; ++channel)
                    motionDelay[(size_t) channel][(size_t) motionWritePosition] = buffer.getSample (channel, sample);

                const float sharedWow = std::sin (wowPhase);
                const float sharedFlutter = std::sin (flutterPhase)
                    + 0.35f * std::sin (flutterPhase * 1.73f + 0.7f);

                if (wearDropoutSamples <= 0 && randomUnipolar() < wearDropout * amount * 0.000015f)
                {
                    wearDropoutSamples = (int) ((0.015f + randomUnipolar() * 0.12f) * (float) sampleRate);
                    wearDropoutTarget = 0.12f + 0.55f * randomUnipolar();
                }
                if (wearDropoutSamples > 0)
                {
                    --wearDropoutSamples;
                    if (wearDropoutSamples == 0)
                        wearDropoutTarget = 1.0f;
                }
                wearDropoutGain += 0.0035f * (wearDropoutTarget - wearDropoutGain);

                for (int channel = 0; channel < 2; ++channel)
                {
                    const float dry = buffer.getSample (channel, sample);
                    const float drift = channel == 0
                        ? 0.0f
                        : wearStereoDrift * (2.2f * std::sin (wowPhase * 0.37f + 1.1f));
                    const float delaySamples = 40.0f + sharedWow * wowDepth + sharedFlutter * flutterDepth + drift;
                    float output = readMotionDelay (channel, delaySamples);
                    output = wearLowpass[(size_t) channel].process (output, cutoffCoefficient);
                    output *= wearDropoutGain;
                    output += randomBipolar() * wearAge * amount * 0.006f;
                    output = dry + amount * (output - dry);
                    buffer.setSample (channel, sample, flushDenorm (output));
                }

                motionWritePosition = (motionWritePosition + 1) % delaySize;
                wowPhase += juce::MathConstants<float>::twoPi * wowRate / (float) sampleRate;
                flutterPhase += juce::MathConstants<float>::twoPi * flutterRate / (float) sampleRate;
                if (wowPhase >= juce::MathConstants<float>::twoPi)
                    wowPhase -= juce::MathConstants<float>::twoPi;
                if (flutterPhase >= juce::MathConstants<float>::twoPi)
                    flutterPhase -= juce::MathConstants<float>::twoPi;

                uiMotion.store (juce::jlimit (-1.0f, 1.0f, sharedWow * wearWow + sharedFlutter * wearFlutter * 0.25f), std::memory_order_relaxed);
                uiDropout.store (1.0f - wearDropoutGain, std::memory_order_relaxed);
            }
        }

        void processSp12 (juce::AudioBuffer<float>& buffer, float amount) noexcept
        {
            const float hostRate = (float) sampleRate;
            const float targetRate = spEffectiveSampleRate();
            const float effectiveRate = juce::jmap (amount, hostRate, targetRate);
            const float increment = effectiveRate / hostRate;
            const float effectiveBits = juce::jmap (amount, 24.0f, 12.0f);
            const float inputCutoff = juce::jmin (12000.0f, effectiveRate * 0.46f);
            const float inputCoefficient = onePoleCoefficient (inputCutoff, hostRate);

            for (int channel = 0; channel < juce::jmin (2, buffer.getNumChannels()); ++channel)
            {
                auto* data = buffer.getWritePointer (channel);
                auto& state = spState[(size_t) channel];
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    const float input = data[sample];
                    const float prefiltered = spInputLowpass[(size_t) channel].process (input, inputCoefficient);
                    const float driven = input + amount * (
                        std::tanh (prefiltered * (1.0f + spInputDrive * 3.5f)) - input);

                    state.phase += increment;
                    if (state.phase >= 1.0)
                    {
                        state.phase -= std::floor (state.phase);
                        state.held = quantise (driven, effectiveBits, false);
                    }

                    float output = state.held;
                    spEnvelope[(size_t) channel] += 0.002f * (std::abs (output) - spEnvelope[(size_t) channel]);

                    if (spFilterMode != SpFilterMode::Unfiltered)
                    {
                        float cutoff = spFilterCutoffHz;
                        if (spFilterMode == SpFilterMode::Dynamic)
                            cutoff *= 0.40f + 0.60f * juce::jlimit (0.0f, 1.0f, spEnvelope[(size_t) channel] * 3.0f);
                        const float coefficient = onePoleCoefficient (cutoff, hostRate);
                        for (auto& stage : state.filter)
                        {
                            stage = coefficient * stage + (1.0f - coefficient) * output;
                            output = stage;
                        }
                    }

                    data[sample] = flushDenorm (input + amount * (output - input));
                }
            }
        }

        float tapeSpeedForMachine() const noexcept
        {
            float speed = tapeSpeedIps (tapeSpeed);
            if (tapeMachine == TapeMachine::Cassette)
                return speed <= 3.75f ? speed : 3.75f;
            return speed < 7.5f ? 7.5f : speed;
        }

        void processTape (juce::AudioBuffer<float>& buffer, float amount) noexcept
        {
            const int samples = buffer.getNumSamples();
            const int delaySize = motionDelay[0].empty() ? 1 : (int) motionDelay[0].size();
            const float speed = tapeSpeedForMachine();
            const bool cassette = tapeMachine == TapeMachine::Cassette;
            const float machineMotion = tapeMotion * amount;
            const float wowRate = cassette ? 0.55f : 0.22f;
            const float flutterRate = cassette ? 8.7f : 5.2f;
            const float wowDepth = machineMotion * (cassette ? 18.0f : 7.0f) * std::sqrt (15.0f / speed);
            const float flutterDepth = machineMotion * (cassette ? 5.0f : 1.8f) * std::sqrt (15.0f / speed);
            const float cutoff = cassette
                ? juce::jmap (tapeAge, 14500.0f, 5200.0f) * std::sqrt (speed / 3.75f)
                : juce::jmap (tapeAge, 20500.0f, 9000.0f) * std::sqrt (speed / 15.0f);
            const float lossCoefficient = onePoleCoefficient (cutoff, (float) sampleRate);
            const float headBumpFrequency = cassette ? 115.0f : 72.0f * (15.0f / speed);
            const float headBumpCoefficient = onePoleCoefficient (headBumpFrequency, (float) sampleRate);
            const float driveGain = 1.0f + amount * tapeDrive * (cassette ? 5.5f : 3.8f);
            const float noiseLevel = tapeNoise * amount * (cassette ? 0.018f : 0.006f);
            const float nrStrengthBase = tapeNoiseReduction == TapeNoiseReduction::BStyle
                ? 0.55f
                : tapeNoiseReduction == TapeNoiseReduction::CStyle ? 0.82f : 0.0f;
            const float nrStrength = nrStrengthBase * tapeNoiseReductionAmount * amount;

            for (int sample = 0; sample < samples; ++sample)
            {
                std::array<float, 2> drySamples {};
                for (int channel = 0; channel < 2; ++channel)
                {
                    const float input = buffer.getSample (channel, sample);
                    drySamples[(size_t) channel] = input;
                    const float nrLow = tapeNrEncodeLow[(size_t) channel].process (
                        input,
                        onePoleCoefficient (cassette ? 1200.0f : 1800.0f, (float) sampleRate));
                    const float nrHigh = input - nrLow;
                    tapeNrEnvelope[(size_t) channel] += 0.0015f
                        * (std::abs (nrHigh) - tapeNrEnvelope[(size_t) channel]);
                    const float lowLevelFactor = 1.0f
                        - juce::jlimit (0.0f, 1.0f, tapeNrEnvelope[(size_t) channel] * 8.0f);
                    const float encoded = input + nrHigh * nrStrength * lowLevelFactor * (cassette ? 2.1f : 1.5f);
                    motionDelay[(size_t) channel][(size_t) motionWritePosition] = encoded;
                }

                const float wow = std::sin (wowPhase)
                    + 0.27f * std::sin (wowPhase * 0.43f + 1.3f);
                const float flutter = std::sin (flutterPhase)
                    + 0.28f * std::sin (flutterPhase * 1.89f + 0.4f);

                for (int channel = 0; channel < 2; ++channel)
                {
                    const float stereoOffset = channel == 0 ? 0.0f : (cassette ? 0.8f : 0.25f) * machineMotion;
                    const float delaySamples = 48.0f + wow * wowDepth + flutter * flutterDepth + stereoOffset;
                    float tapeInput = readMotionDelay (channel, delaySamples);

                    tapeMemory[(size_t) channel] += (cassette ? 0.024f : 0.012f)
                        * (tapeInput - tapeMemory[(size_t) channel]);
                    const float magneticInput = tapeInput * driveGain
                        + tapeMemory[(size_t) channel] * tapeAge * 0.35f;
                    float output = std::tanh (magneticInput)
                        / juce::jmax (0.001f, std::tanh (driveGain));

                    const float bumpLow = tapeHeadBumpLow[(size_t) channel].process (output, headBumpCoefficient);
                    const float bumpBand = bumpLow - tapeHeadBumpBand[(size_t) channel].process (bumpLow, headBumpCoefficient);
                    output += bumpBand * amount * (cassette ? 0.08f : 0.18f) * (1.0f - speed / 45.0f);
                    output = tapeLowpass[(size_t) channel].process (output, lossCoefficient);

                    float noise = randomBipolar();
                    tapeNoiseLow[(size_t) channel] += 0.04f * (noise - tapeNoiseLow[(size_t) channel]);
                    if (cassette)
                        noise -= tapeNoiseLow[(size_t) channel] * 0.82f;
                    else
                        noise = tapeNoiseLow[(size_t) channel] * 0.58f + noise * 0.42f;
                    output += noise * noiseLevel;

                    const float decodedLow = tapeNrDecodeLow[(size_t) channel].process (
                        output,
                        onePoleCoefficient (cassette ? 1200.0f : 1800.0f, (float) sampleRate));
                    float decodedHigh = output - decodedLow;
                    const float decodeEnvelope = std::abs (decodedHigh);
                    const float decodeLowLevel = 1.0f
                        - juce::jlimit (0.0f, 1.0f, decodeEnvelope * 8.0f);
                    decodedHigh /= 1.0f + nrStrength * decodeLowLevel * (cassette ? 2.1f : 1.5f);
                    output = decodedLow + decodedHigh;

                    tapeDenoiseEnvelope[(size_t) channel] += 0.0008f
                        * (std::abs (decodedHigh) - tapeDenoiseEnvelope[(size_t) channel]);
                    const float denoiseThreshold = cassette ? 0.018f : 0.007f;
                    const float denoiseGate = juce::jlimit (
                        0.0f,
                        1.0f,
                        tapeDenoiseEnvelope[(size_t) channel] / denoiseThreshold);
                    output = decodedLow + decodedHigh
                        * juce::jmap (tapeDenoise * amount, 1.0f, denoiseGate);
                    output = drySamples[(size_t) channel]
                        + amount * (output - drySamples[(size_t) channel]);

                    buffer.setSample (channel, sample, flushDenorm (output));
                }

                motionWritePosition = (motionWritePosition + 1) % delaySize;
                wowPhase += juce::MathConstants<float>::twoPi * wowRate / (float) sampleRate;
                flutterPhase += juce::MathConstants<float>::twoPi * flutterRate / (float) sampleRate;
                if (wowPhase >= juce::MathConstants<float>::twoPi)
                    wowPhase -= juce::MathConstants<float>::twoPi;
                if (flutterPhase >= juce::MathConstants<float>::twoPi)
                    flutterPhase -= juce::MathConstants<float>::twoPi;

                uiMotion.store (juce::jlimit (-1.0f, 1.0f, wow * 0.75f + flutter * 0.25f), std::memory_order_relaxed);
            }
        }

        void processVinyl (juce::AudioBuffer<float>& buffer, float amount) noexcept
        {
            const float cutoff = juce::jmap (vinylWear * amount, 19000.0f, 5500.0f);
            const float coefficient = onePoleCoefficient (cutoff, (float) sampleRate);
            const float surfaceLevel = vinylSurface * amount * 0.012f;

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const bool dustEvent = randomUnipolar() < vinylDust * amount * 0.00008f;
                const bool crackleEvent = randomUnipolar() < vinylCrackle * amount * 0.00045f;

                for (int channel = 0; channel < juce::jmin (2, buffer.getNumChannels()); ++channel)
                {
                    const float dry = buffer.getSample (channel, sample);
                    float output = vinylLowpass[(size_t) channel].process (dry, coefficient);

                    float surface = randomBipolar();
                    const float surfaceLow = vinylHighpassLow[(size_t) channel].process (
                        surface,
                        onePoleCoefficient (1800.0f, (float) sampleRate));
                    surface -= surfaceLow;

                    if (dustEvent)
                        vinylCrackleEnvelope[(size_t) channel] += randomBipolar() * 0.25f;
                    if (crackleEvent)
                        vinylCrackleEnvelope[(size_t) channel] += randomBipolar() * 0.65f;
                    vinylCrackleEnvelope[(size_t) channel] *= 0.91f;

                    output += surface * surfaceLevel
                        + vinylCrackleEnvelope[(size_t) channel];
                    output = dry + amount * (output - dry);
                    buffer.setSample (channel, sample, flushDenorm (output));
                }
            }
        }

        double sampleRate = 44100.0;
        int maxBlockSize = 512;
        RetroMode mode = RetroMode::Bitcrush;
        float mix = 1.0f;

        int bitDepth = 12;
        float bitSampleRateHz = 12000.0f;
        BitHoldMode bitHoldMode = BitHoldMode::Step;
        bool bitDither = false;
        bool bitAntiAlias = false;

        float lossyBandwidthHz = 12000.0f;
        float lossyDetail = 0.65f;
        float lossyDamage = 0.35f;
        LossyQuality lossyQuality = LossyQuality::Normal;
        bool lossyStereoLink = true;

        float wearWow = 0.30f;
        float wearFlutter = 0.20f;
        float wearDropout = 0.10f;
        float wearAge = 0.35f;
        float wearStereoDrift = 0.20f;

        int spClockSemitones = 0;
        SpFilterMode spFilterMode = SpFilterMode::Static;
        float spFilterCutoffHz = 8500.0f;
        float spInputDrive = 0.30f;

        TapeMachine tapeMachine = TapeMachine::ReelToReel;
        TapeSpeed tapeSpeed = TapeSpeed::Ips15;
        float tapeDrive = 0.35f;
        float tapeAge = 0.25f;
        float tapeMotion = 0.18f;
        float tapeNoise = 0.12f;
        TapeNoiseReduction tapeNoiseReduction = TapeNoiseReduction::Off;
        float tapeNoiseReductionAmount = 0.70f;
        float tapeDenoise = 0.0f;

        float vinylDust = 0.15f;
        float vinylCrackle = 0.12f;
        float vinylSurface = 0.18f;
        float vinylWear = 0.25f;

        juce::dsp::DryWetMixer<float> dryWetMixer { 2048 };
        std::array<std::unique_ptr<SpectralLossy>, 3> lossyProcessors;
        std::array<BitState, 2> bitState;
        std::array<SpState, 2> spState;
        std::array<std::vector<float>, 2> motionDelay;
        int motionWritePosition = 0;
        float wowPhase = 0.0f;
        float flutterPhase = 0.0f;

        std::array<OnePole, 2> bitAntiAliasLowpass;
        std::array<OnePole, 2> wearLowpass;
        float wearDropoutGain = 1.0f;
        float wearDropoutTarget = 1.0f;
        int wearDropoutSamples = 0;

        std::array<OnePole, 2> spInputLowpass;
        std::array<float, 2> spEnvelope {};

        std::array<OnePole, 2> tapeLowpass;
        std::array<OnePole, 2> tapeHeadBumpLow;
        std::array<OnePole, 2> tapeHeadBumpBand;
        std::array<float, 2> tapeMemory {};
        std::array<float, 2> tapeNoiseLow {};
        std::array<OnePole, 2> tapeNrEncodeLow;
        std::array<OnePole, 2> tapeNrDecodeLow;
        std::array<float, 2> tapeNrEnvelope {};
        std::array<float, 2> tapeDenoiseEnvelope {};
        float tapeDropoutGain = 1.0f;
        float tapeDropoutTarget = 1.0f;
        int tapeDropoutSamples = 0;

        std::array<OnePole, 2> vinylLowpass;
        std::array<OnePole, 2> vinylHighpassLow;
        std::array<float, 2> vinylCrackleEnvelope {};

        std::mt19937 random { 0x4d465831u };
        std::uniform_real_distribution<float> randomDistribution { -1.0f, 1.0f };
        int lastMode = -1;
        int lastLossyQuality = -1;
    };
}
