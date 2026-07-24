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

        void setLossyParams (
            float rangeHzToUse,
            float detailToUse,
            float damageToUse,
            float scrambleToUse,
            float rateHzToUse,
            int qualityIndex,
            bool stereoLinkToUse) noexcept
        {
            lossyRangeHz = juce::jlimit (
                500.0f,
                24000.0f,
                rangeHzToUse);
            lossyDetail = juce::jlimit (
                0.0f,
                1.0f,
                detailToUse);
            lossyDamage = juce::jlimit (
                0.0f,
                1.0f,
                damageToUse);
            lossyScramble = juce::jlimit (
                0.0f,
                1.0f,
                scrambleToUse);
            lossyRateHz = juce::jlimit (
                0.05f,
                20.0f,
                rateHzToUse);
            lossyQuality = (LossyQuality)
                juce::jlimit (
                    0,
                    2,
                    qualityIndex);
            lossyStereoLink =
                stereoLinkToUse;
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
            SpectralLossy (int orderToUse,
                           RetroEffect& ownerToUse)
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
                const int bins = fftSize / 2 + 1;

                for (int channel = 0;
                     channel < 2;
                     ++channel)
                {
                    inputRing[(size_t) channel].assign (
                        (size_t) fftSize,
                        0.0f);
                    outputRing[(size_t) channel].assign (
                        (size_t) outputSize,
                        0.0f);
                    fftData[(size_t) channel].assign (
                        (size_t) fftSize * 2,
                        0.0f);
                    sourceReal[(size_t) channel].assign (
                        (size_t) bins,
                        0.0f);
                    sourceImaginary[(size_t) channel].assign (
                        (size_t) bins,
                        0.0f);
                }

                window.resize ((size_t) fftSize);

                for (int index = 0;
                     index < fftSize;
                     ++index)
                {
                    const float phase =
                        juce::MathConstants<float>::twoPi
                        * (float) index
                        / (float) fftSize;

                    // Square-root Hann: the analysis and synthesis windows
                    // multiply back to Hann. With a 25% hop, four Hann
                    // windows sum to 2.0, so the overlap factor is 0.5.
                    window[(size_t) index] =
                        std::sqrt (
                            0.5f
                            - 0.5f
                                * std::cos (phase));
                }

                // JUCE may select different FFT engines per platform.
                // Measure the forward/inverse round-trip once so overlap-add
                // gain never depends on FFT size or backend normalisation.
                std::vector<float> probe (
                    (size_t) fftSize * 2,
                    0.0f);
                probe[0] = 1.0f;
                fft.performRealOnlyForwardTransform (
                    probe.data(),
                    true);
                fft.performRealOnlyInverseTransform (
                    probe.data());

                const float roundTripGain =
                    std::abs (probe[0]);

                inverseRoundTripScale =
                    roundTripGain > 1.0e-9f
                        ? 1.0f / roundTripGain
                        : 1.0f / (float) fftSize;

                overlapNormalisation =
                    inverseRoundTripScale * 0.5f;

                reset();
            }

            void reset() noexcept
            {
                for (auto& ring : inputRing)
                    std::fill (
                        ring.begin(),
                        ring.end(),
                        0.0f);

                for (auto& ring : outputRing)
                    std::fill (
                        ring.begin(),
                        ring.end(),
                        0.0f);

                inputWrite = 0;
                outputRead = 0;
                samplesUntilFrame = fftSize;
                frameCounter = 0;
                energyCompensation = {
                    1.0f,
                    1.0f
                };
            }

            int getLatencySamples() const noexcept
            {
                return fftSize;
            }

            void process (
                juce::AudioBuffer<float>& buffer,
                float amount,
                float rangeHz,
                float detail,
                float damage,
                float scramble,
                float rateHz,
                bool stereoLink) noexcept
            {
                const int channels =
                    juce::jmin (
                        2,
                        buffer.getNumChannels());
                const int samples =
                    buffer.getNumSamples();
                const int outputSize =
                    fftSize * 4;

                for (int sample = 0;
                     sample < samples;
                     ++sample)
                {
                    for (int channel = 0;
                         channel < channels;
                         ++channel)
                    {
                        auto* data =
                            buffer.getWritePointer (
                                channel);

                        inputRing[(size_t) channel]
                                 [(size_t) inputWrite] =
                            data[sample];

                        data[sample] =
                            outputRing[(size_t) channel]
                                      [(size_t) outputRead];

                        outputRing[(size_t) channel]
                                  [(size_t) outputRead] =
                            0.0f;
                    }

                    inputWrite =
                        (inputWrite + 1)
                        % fftSize;
                    outputRead =
                        (outputRead + 1)
                        % outputSize;

                    if (--samplesUntilFrame <= 0)
                    {
                        samplesUntilFrame =
                            hopSize;

                        processFrame (
                            channels,
                            amount,
                            rangeHz,
                            detail,
                            damage,
                            scramble,
                            rateHz,
                            stereoLink);

                        ++frameCounter;
                    }
                }
            }

        private:
            static std::uint32_t hashValue (
                std::uint32_t value) noexcept
            {
                value ^= value >> 16;
                value *= 0x7feb352du;
                value ^= value >> 15;
                value *= 0x846ca68bu;
                value ^= value >> 16;
                return value;
            }

            static float smoothStep (
                float value) noexcept
            {
                const float clamped =
                    juce::jlimit (
                        0.0f,
                        1.0f,
                        value);

                return clamped
                    * clamped
                    * (3.0f - 2.0f * clamped);
            }

            static float interpolate (
                float a,
                float b,
                float amountToUse) noexcept
            {
                return a
                    + (b - a)
                        * amountToUse;
            }

            int mappedBin (
                std::uint64_t mapIndex,
                int bin,
                int channel,
                int activeUpperBin,
                float scrambleAmount) const noexcept
            {
                const std::uint32_t linkChannel =
                    (std::uint32_t) channel;

                const std::uint32_t noise =
                    hashValue (
                        (std::uint32_t) mapIndex
                            * 2246822519u
                        ^ (std::uint32_t) bin
                            * 2654435761u
                        ^ linkChannel
                            * 3266489917u);

                const float randomSigned =
                    (float) (
                        (int) (noise & 0x0000ffffu)
                        - 32768)
                    / 32768.0f;

                const int maximumOffset =
                    juce::jmax (
                        1,
                        juce::roundToInt (
                            scrambleAmount
                            * (float) juce::jmin (
                                96,
                                juce::jmax (
                                    2,
                                    activeUpperBin
                                        / 3))));

                return juce::jlimit (
                    1,
                    activeUpperBin,
                    bin
                        + juce::roundToInt (
                            randomSigned
                            * (float) maximumOffset));
            }

            void processFrame (
                int channels,
                float amount,
                float rangeHz,
                float detail,
                float damage,
                float scramble,
                float rateHz,
                bool stereoLink) noexcept
            {
                std::array<float, spectrumBands>
                    inputDisplay {};
                std::array<float, spectrumBands>
                    outputDisplay {};

                const int nyquistBin =
                    fftSize / 2;
                const float nyquistFrequency =
                    (float) sampleRate * 0.5f;
                const float safeRange =
                    juce::jlimit (
                        80.0f,
                        nyquistFrequency,
                        rangeHz);
                const int activeUpperBin =
                    juce::jlimit (
                        2,
                        nyquistBin,
                        juce::roundToInt (
                            safeRange
                            * (float) fftSize
                            / (float) sampleRate));

                const float safeAmount =
                    juce::jlimit (
                        0.0f,
                        1.0f,
                        amount);
                const float detailLoss =
                    safeAmount
                    * (1.0f
                       - juce::jlimit (
                           0.0f,
                           1.0f,
                           detail));
                const float damageAmount =
                    safeAmount
                    * juce::jlimit (
                        0.0f,
                        1.0f,
                        damage);
                const float scrambleAmount =
                    safeAmount
                    * juce::jlimit (
                        0.0f,
                        1.0f,
                        scramble);

                const float safeRate =
                    juce::jlimit (
                        0.05f,
                        20.0f,
                        rateHz);
                const int framesPerMap =
                    juce::jmax (
                        1,
                        juce::roundToInt (
                            (float) sampleRate
                            / ((float) hopSize
                               * safeRate)));

                const std::uint64_t mapIndex =
                    frameCounter
                    / (std::uint64_t)
                        framesPerMap;
                const float mapProgress =
                    (float) (
                        frameCounter
                        % (std::uint64_t)
                            framesPerMap)
                    / (float) framesPerMap;
                const float mapBlend =
                    smoothStep (mapProgress);

                const int maximumGroupWidth =
                    juce::jmax (
                        1,
                        activeUpperBin / 20);
                const int groupWidth =
                    1
                    + juce::roundToInt (
                        detailLoss
                        * detailLoss
                        * (float) maximumGroupWidth);

                for (int channel = 0;
                     channel < channels;
                     ++channel)
                {
                    auto& data =
                        fftData[(size_t) channel];
                    auto& originalReal =
                        sourceReal[(size_t) channel];
                    auto& originalImaginary =
                        sourceImaginary[(size_t) channel];

                    std::fill (
                        data.begin(),
                        data.end(),
                        0.0f);

                    for (int index = 0;
                         index < fftSize;
                         ++index)
                    {
                        const int ringIndex =
                            (inputWrite + index)
                            % fftSize;

                        data[(size_t) index] =
                            inputRing[(size_t) channel]
                                     [(size_t) ringIndex]
                            * window[(size_t) index];
                    }

                    fft.performRealOnlyForwardTransform (
                        data.data(),
                        true);

                    double inputEnergy = 0.0;

                    for (int bin = 0;
                         bin <= nyquistBin;
                         ++bin)
                    {
                        const bool dcBin =
                            bin == 0;
                        const bool lastBin =
                            bin == nyquistBin;
                        const int realIndex =
                            dcBin
                                ? 0
                                : lastBin
                                    ? 1
                                    : bin * 2;
                        const int imaginaryIndex =
                            bin * 2 + 1;

                        const float real =
                            data[(size_t) realIndex];
                        const float imaginary =
                            (dcBin || lastBin)
                                ? 0.0f
                                : data[(size_t)
                                    imaginaryIndex];

                        originalReal[(size_t) bin] =
                            real;
                        originalImaginary[
                            (size_t) bin] =
                            imaginary;

                        inputEnergy +=
                            (double) real * real
                            + (double) imaginary
                                * imaginary;

                        const float frequency =
                            (float) bin
                            * (float) sampleRate
                            / (float) fftSize;

                        const int displayBand =
                            juce::jlimit (
                                0,
                                spectrumBands - 1,
                                (int) std::floor (
                                    std::log2 (
                                        juce::jmax (
                                            20.0f,
                                            frequency)
                                        / 20.0f)
                                    / std::log2 (
                                        24000.0f
                                        / 20.0f)
                                    * (float)
                                        spectrumBands));

                        inputDisplay[
                            (size_t) displayBand] =
                            juce::jmax (
                                inputDisplay[
                                    (size_t)
                                        displayBand],
                                std::sqrt (
                                    real * real
                                    + imaginary
                                        * imaginary));
                    }

                    double outputEnergy = 0.0;
                    const int mapChannel =
                        stereoLink
                            ? 0
                            : channel;

                    for (int bin = 0;
                         bin <= nyquistBin;
                         ++bin)
                    {
                        const bool dcBin =
                            bin == 0;
                        const bool lastBin =
                            bin == nyquistBin;
                        const int realIndex =
                            dcBin
                                ? 0
                                : lastBin
                                    ? 1
                                    : bin * 2;
                        const int imaginaryIndex =
                            bin * 2 + 1;

                        const float originalR =
                            originalReal[
                                (size_t) bin];
                        const float originalI =
                            originalImaginary[
                                (size_t) bin];

                        float processedR =
                            originalR;
                        float processedI =
                            originalI;

                        if (! dcBin
                            && ! lastBin
                            && bin <= activeUpperBin
                            && safeAmount
                                > 1.0e-7f)
                        {
                            const int groupedBin =
                                juce::jlimit (
                                    1,
                                    activeUpperBin,
                                    (bin / groupWidth)
                                        * groupWidth
                                        + groupWidth / 2);

                            const float groupedR =
                                originalReal[
                                    (size_t)
                                        groupedBin];
                            const float groupedI =
                                originalImaginary[
                                    (size_t)
                                        groupedBin];

                            processedR =
                                interpolate (
                                    processedR,
                                    groupedR,
                                    detailLoss);
                            processedI =
                                interpolate (
                                    processedI,
                                    groupedI,
                                    detailLoss);

                            const int mappedA =
                                mappedBin (
                                    mapIndex,
                                    groupedBin,
                                    mapChannel,
                                    activeUpperBin,
                                    scrambleAmount);
                            const int mappedB =
                                mappedBin (
                                    mapIndex + 1u,
                                    groupedBin,
                                    mapChannel,
                                    activeUpperBin,
                                    scrambleAmount);

                            const float scrambledR =
                                interpolate (
                                    originalReal[
                                        (size_t)
                                            mappedA],
                                    originalReal[
                                        (size_t)
                                            mappedB],
                                    mapBlend);
                            const float scrambledI =
                                interpolate (
                                    originalImaginary[
                                        (size_t)
                                            mappedA],
                                    originalImaginary[
                                        (size_t)
                                            mappedB],
                                    mapBlend);

                            processedR =
                                interpolate (
                                    processedR,
                                    scrambledR,
                                    scrambleAmount);
                            processedI =
                                interpolate (
                                    processedI,
                                    scrambledI,
                                    scrambleAmount);

                            const std::uint32_t
                                damageNoise =
                                    hashValue (
                                        (std::uint32_t)
                                            frameCounter
                                            * 1315423911u
                                        ^ (std::uint32_t)
                                            bin
                                            * 374761393u
                                        ^ (std::uint32_t)
                                            mapChannel
                                            * 668265263u);

                            const float random01 =
                                (float) (
                                    damageNoise
                                    & 0x00ffffffu)
                                / 16777215.0f;
                            const float
                                dropoutProbability =
                                    damageAmount
                                    * damageAmount
                                    * 0.34f;

                            if (random01
                                < dropoutProbability
                                && bin > 2)
                            {
                                const float
                                    retainedGain =
                                        1.0f
                                        - 0.94f
                                            * damageAmount;
                                processedR *=
                                    retainedGain;
                                processedI *=
                                    retainedGain;
                            }
                            else if (damageAmount
                                     > 1.0e-5f)
                            {
                                const int neighbour =
                                    juce::jlimit (
                                        1,
                                        activeUpperBin,
                                        bin
                                            + ((damageNoise
                                                & 1u)
                                                != 0u
                                                ? 1
                                                : -1));
                                const float merge =
                                    damageAmount
                                    * 0.24f;

                                processedR =
                                    interpolate (
                                        processedR,
                                        originalReal[
                                            (size_t)
                                                neighbour],
                                        merge);
                                processedI =
                                    interpolate (
                                        processedI,
                                        originalImaginary[
                                            (size_t)
                                                neighbour],
                                        merge);
                            }

                            if (detailLoss
                                > 1.0e-6f)
                            {
                                float magnitude =
                                    std::sqrt (
                                        processedR
                                            * processedR
                                        + processedI
                                            * processedI);
                                float phase =
                                    std::atan2 (
                                        processedI,
                                        processedR);

                                const float
                                    magnitudeSteps =
                                        8.0f
                                        + 4088.0f
                                            * std::pow (
                                                1.0f
                                                    - detailLoss,
                                                2.0f);
                                const float
                                    phaseSteps =
                                        8.0f
                                        + 1016.0f
                                            * std::pow (
                                                1.0f
                                                    - detailLoss,
                                                2.0f);

                                const float
                                    quantisedMagnitude =
                                        magnitude
                                            > 1.0e-12f
                                            ? std::pow (
                                                2.0f,
                                                std::round (
                                                    std::log2 (
                                                        magnitude
                                                        + 1.0e-12f)
                                                    * magnitudeSteps)
                                                    / magnitudeSteps)
                                            : 0.0f;

                                const float
                                    quantisedPhase =
                                        std::round (
                                            phase
                                            / juce::MathConstants<
                                                float>::twoPi
                                            * phaseSteps)
                                        / phaseSteps
                                        * juce::MathConstants<
                                            float>::twoPi;

                                magnitude =
                                    interpolate (
                                        magnitude,
                                        quantisedMagnitude,
                                        detailLoss);
                                phase =
                                    interpolate (
                                        phase,
                                        quantisedPhase,
                                        detailLoss);

                                processedR =
                                    magnitude
                                    * std::cos (phase);
                                processedI =
                                    magnitude
                                    * std::sin (phase);
                            }
                        }

                        data[(size_t) realIndex] =
                            processedR;

                        if (! dcBin
                            && ! lastBin)
                        {
                            data[(size_t)
                                imaginaryIndex] =
                                processedI;
                        }

                        outputEnergy +=
                            (double) processedR
                                * processedR
                            + (double) processedI
                                * processedI;
                    }

                    const float targetCompensation =
                        juce::jlimit (
                            0.63f,
                            1.58f,
                            (float) std::sqrt (
                                inputEnergy
                                / juce::jmax (
                                    1.0e-18,
                                    outputEnergy)));

                    energyCompensation[
                        (size_t) channel] +=
                        0.10f
                        * (targetCompensation
                           - energyCompensation[
                               (size_t) channel]);

                    const float appliedCompensation =
                        1.0f
                        + safeAmount
                            * 0.85f
                            * (energyCompensation[
                                   (size_t) channel]
                               - 1.0f);

                    for (int bin = 1;
                         bin <= nyquistBin;
                         ++bin)
                    {
                        const bool lastBin =
                            bin == nyquistBin;
                        const int realIndex =
                            lastBin
                                ? 1
                                : bin * 2;
                        const int imaginaryIndex =
                            bin * 2 + 1;

                        data[(size_t) realIndex] *=
                            appliedCompensation;

                        if (! lastBin)
                        {
                            data[(size_t)
                                imaginaryIndex] *=
                                appliedCompensation;
                        }

                        const float real =
                            data[(size_t) realIndex];
                        const float imaginary =
                            lastBin
                                ? 0.0f
                                : data[(size_t)
                                    imaginaryIndex];
                        const float frequency =
                            (float) bin
                            * (float) sampleRate
                            / (float) fftSize;
                        const int displayBand =
                            juce::jlimit (
                                0,
                                spectrumBands - 1,
                                (int) std::floor (
                                    std::log2 (
                                        juce::jmax (
                                            20.0f,
                                            frequency)
                                        / 20.0f)
                                    / std::log2 (
                                        24000.0f
                                        / 20.0f)
                                    * (float)
                                        spectrumBands));

                        outputDisplay[
                            (size_t) displayBand] =
                            juce::jmax (
                                outputDisplay[
                                    (size_t)
                                        displayBand],
                                std::sqrt (
                                    real * real
                                    + imaginary
                                        * imaginary));
                    }

                    fft.performRealOnlyInverseTransform (
                        data.data());

                    const int outputSize =
                        fftSize * 4;

                    for (int index = 0;
                         index < fftSize;
                         ++index)
                    {
                        const int target =
                            (outputRead + index)
                            % outputSize;

                        outputRing[(size_t) channel]
                                  [(size_t) target] +=
                            data[(size_t) index]
                            * window[(size_t) index]
                            * overlapNormalisation;
                    }
                }

                float inputPeak = 1.0e-9f;

                for (int band = 0;
                     band < spectrumBands;
                     ++band)
                {
                    inputPeak =
                        juce::jmax (
                            inputPeak,
                            inputDisplay[
                                (size_t) band]);
                }

                for (int band = 0;
                     band < spectrumBands;
                     ++band)
                {
                    owner.uiInputSpectrum[
                        (size_t) band].store (
                            juce::jlimit (
                                0.0f,
                                1.0f,
                                inputDisplay[
                                    (size_t) band]
                                / inputPeak),
                            std::memory_order_relaxed);

                    owner.uiOutputSpectrum[
                        (size_t) band].store (
                            juce::jlimit (
                                0.0f,
                                1.0f,
                                outputDisplay[
                                    (size_t) band]
                                / inputPeak),
                            std::memory_order_relaxed);
                }
            }

            int order = 9;
            int fftSize = 512;
            int hopSize = 128;
            double sampleRate = 44100.0;
            juce::dsp::FFT fft;
            RetroEffect& owner;

            std::array<std::vector<float>, 2>
                inputRing;
            std::array<std::vector<float>, 2>
                outputRing;
            std::array<std::vector<float>, 2>
                fftData;
            std::array<std::vector<float>, 2>
                sourceReal;
            std::array<std::vector<float>, 2>
                sourceImaginary;
            std::vector<float> window;

            float inverseRoundTripScale = 1.0f;
            float overlapNormalisation = 0.5f;
            std::array<float, 2>
                energyCompensation {
                    1.0f,
                    1.0f
                };

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
                    const float dry = data[sample];

                    // Amount zero must be exactly transparent, regardless of
                    // anti-aliasing or the selected hold/interpolation mode.
                    // Keep the state primed with the current signal so that
                    // re-enabling Bitcrush does not replay a stale sample.
                    if (amount <= 1.0e-6f)
                    {
                        state.phase = 1.0;
                        state.previous = dry;
                        state.held = dry;
                        state.smooth = dry;
                        data[sample] = dry;
                        continue;
                    }

                    float input = dry;
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

        void processLossy (
            juce::AudioBuffer<float>& buffer,
            float amount) noexcept
        {
            const int index =
                juce::jlimit (
                    0,
                    2,
                    (int) lossyQuality);
            auto& processor =
                lossyProcessors[(size_t) index];

            if (processor != nullptr)
            {
                processor->process (
                    buffer,
                    amount,
                    lossyRangeHz,
                    lossyDetail,
                    lossyDamage,
                    lossyScramble,
                    lossyRateHz,
                    lossyStereoLink);
            }
        }

        void processWear (
            juce::AudioBuffer<float>& buffer,
            float amount) noexcept
        {
            const int samples =
                buffer.getNumSamples();
            const int delaySize =
                motionDelay[0].empty()
                    ? 1
                    : (int)
                        motionDelay[0].size();
            const float rate =
                (float) sampleRate;
            const float scale =
                juce::jlimit (
                    0.0f,
                    1.0f,
                    amount);
            const float effectiveAge =
                wearAge * scale;

            const float wowRate =
                0.10f
                + 1.05f
                    * std::pow (
                        wearWow,
                        0.80f);
            const float flutterRate =
                3.5f
                + 13.5f
                    * std::pow (
                        wearFlutter,
                        0.75f);

            const float wowDepthMs =
                scale
                * std::pow (
                    wearWow,
                    1.18f)
                * (0.55f
                   + 5.45f
                       * effectiveAge);
            const float flutterDepthMs =
                scale
                * std::pow (
                    wearFlutter,
                    1.12f)
                * (0.08f
                   + 1.02f
                       * effectiveAge);
            const float stereoDepthMs =
                scale
                * wearStereoDrift
                * (0.10f
                   + 0.90f
                       * effectiveAge);

            // The Retro Amount control is a parameter scale, not another
            // dry/wet control. At zero, the virtual transport delay is zero;
            // at one, the selected motion parameters are reached.
            const float baseDelaySamples =
                scale
                * 8.0f
                * 0.001f
                * rate;
            const float wowDepthSamples =
                wowDepthMs
                * 0.001f
                * rate;
            const float flutterDepthSamples =
                flutterDepthMs
                * 0.001f
                * rate;
            const float stereoDepthSamples =
                stereoDepthMs
                * 0.001f
                * rate;

            const float cutoff =
                juce::jmap (
                    effectiveAge,
                    19000.0f,
                    2900.0f);
            const float cutoffCoefficient =
                onePoleCoefficient (
                    cutoff,
                    rate);

            const float dropoutAttackSeconds =
                0.006f
                + 0.020f
                    * (1.0f
                       - wearDropout);
            const float dropoutReleaseSeconds =
                0.085f
                + 0.170f
                    * (1.0f
                       - wearDropout);
            const float attackCoefficient =
                1.0f
                - std::exp (
                    -1.0f
                    / juce::jmax (
                        1.0f,
                        rate
                            * dropoutAttackSeconds));
            const float releaseCoefficient =
                1.0f
                - std::exp (
                    -1.0f
                    / juce::jmax (
                        1.0f,
                        rate
                            * dropoutReleaseSeconds));

            for (int sample = 0;
                 sample < samples;
                 ++sample)
            {
                for (int channel = 0;
                     channel < 2;
                     ++channel)
                {
                    motionDelay[(size_t) channel]
                               [(size_t)
                                    motionWritePosition] =
                        buffer.getSample (
                            channel,
                            sample);
                }

                const float sharedWow =
                    std::sin (wowPhase)
                    + 0.22f
                        * std::sin (
                            wowPhase
                                * 0.47f
                            + 1.13f);
                const float sharedFlutter =
                    std::sin (flutterPhase)
                    + 0.38f
                        * std::sin (
                            flutterPhase
                                * 1.73f
                            + 0.70f)
                    + 0.17f
                        * std::sin (
                            flutterPhase
                                * 2.91f
                            + 2.20f);

                if (wearDropoutSamples <= 0
                    && randomUnipolar()
                        < wearDropout
                            * scale
                            * 0.000025f)
                {
                    wearDropoutSamples =
                        (int) (
                            (0.030f
                             + randomUnipolar()
                                 * 0.190f)
                            * rate);
                    wearDropoutTarget =
                        0.02f
                        + 0.43f
                            * randomUnipolar();
                }

                if (wearDropoutSamples > 0)
                {
                    --wearDropoutSamples;

                    if (wearDropoutSamples == 0)
                        wearDropoutTarget = 1.0f;
                }

                const float dropoutCoefficient =
                    wearDropoutTarget
                            < wearDropoutGain
                        ? attackCoefficient
                        : releaseCoefficient;

                wearDropoutGain +=
                    dropoutCoefficient
                    * (wearDropoutTarget
                       - wearDropoutGain);

                for (int channel = 0;
                     channel < 2;
                     ++channel)
                {
                    const float dry =
                        buffer.getSample (
                            channel,
                            sample);
                    const float driftPolarity =
                        channel == 0
                            ? -1.0f
                            : 1.0f;
                    const float drift =
                        driftPolarity
                        * stereoDepthSamples
                        * std::sin (
                            wowPhase
                                * 0.37f
                            + (channel == 0
                                ? 0.25f
                                : 1.55f));

                    const float delaySamples =
                        baseDelaySamples
                        + sharedWow
                            * wowDepthSamples
                        + sharedFlutter
                            * flutterDepthSamples
                        + drift;

                    float output =
                        scale <= 1.0e-7f
                            ? dry
                            : readMotionDelay (
                                channel,
                                juce::jmax (
                                    0.0f,
                                    delaySamples));

                    const float filtered =
                        wearLowpass[
                            (size_t) channel]
                            .process (
                                output,
                                cutoffCoefficient);

                    // Age scales only the head-loss parameter. It is not an
                    // implicit wet mix for the complete Wear processor.
                    output +=
                        effectiveAge
                        * (filtered - output);

                    const float effectiveDropoutGain =
                        1.0f
                        + scale
                            * (wearDropoutGain
                               - 1.0f);
                    output *=
                        effectiveDropoutGain;
                    output +=
                        randomBipolar()
                        * effectiveAge
                        * 0.0075f;

                    buffer.setSample (
                        channel,
                        sample,
                        flushDenorm (output));
                }

                motionWritePosition =
                    (motionWritePosition + 1)
                    % delaySize;

                wowPhase +=
                    juce::MathConstants<float>::twoPi
                    * wowRate
                    / rate;
                flutterPhase +=
                    juce::MathConstants<float>::twoPi
                    * flutterRate
                    / rate;

                if (wowPhase
                    >= juce::MathConstants<
                        float>::twoPi)
                {
                    wowPhase -=
                        juce::MathConstants<
                            float>::twoPi;
                }

                if (flutterPhase
                    >= juce::MathConstants<
                        float>::twoPi)
                {
                    flutterPhase -=
                        juce::MathConstants<
                            float>::twoPi;
                }

                const float displayedMotion =
                    scale
                    * (sharedWow
                           * wearWow
                           * 0.72f
                       + sharedFlutter
                           * wearFlutter
                           * 0.28f);

                uiMotion.store (
                    juce::jlimit (
                        -1.0f,
                        1.0f,
                        displayedMotion),
                    std::memory_order_relaxed);
                uiDropout.store (
                    juce::jlimit (
                        0.0f,
                        1.0f,
                        1.0f
                            - effectiveDropoutGain),
                    std::memory_order_relaxed);
            }
        }

        void processSp12 (
            juce::AudioBuffer<float>& buffer,
            float amount) noexcept
        {
            const float hostRate =
                (float) sampleRate;
            const float scale =
                juce::jlimit (
                    0.0f,
                    1.0f,
                    amount);
            const float targetRate =
                spEffectiveSampleRate();
            const float effectiveRate =
                juce::jmap (
                    scale,
                    hostRate,
                    targetRate);
            const float increment =
                effectiveRate / hostRate;
            const float effectiveBits =
                juce::jmap (
                    scale,
                    24.0f,
                    12.0f);
            const float inputCutoff =
                juce::jmin (
                    12000.0f,
                    effectiveRate * 0.46f);
            const float inputCoefficient =
                onePoleCoefficient (
                    inputCutoff,
                    hostRate);

            for (int channel = 0;
                 channel < juce::jmin (
                     2,
                     buffer.getNumChannels());
                 ++channel)
            {
                auto* data =
                    buffer.getWritePointer (
                        channel);
                auto& state =
                    spState[(size_t) channel];

                for (int sample = 0;
                     sample < buffer.getNumSamples();
                     ++sample)
                {
                    const float input =
                        data[sample];

                    if (scale <= 1.0e-7f)
                    {
                        state.phase = 1.0;
                        state.held = input;
                        data[sample] = input;
                        continue;
                    }

                    const float prefiltered =
                        spInputLowpass[
                            (size_t) channel]
                            .process (
                                input,
                                inputCoefficient);
                    const float driveAmount =
                        spInputDrive * scale;
                    const float saturated =
                        std::tanh (
                            prefiltered
                            * (1.0f
                               + driveAmount
                                   * 3.5f));
                    const float driven =
                        input
                        + driveAmount
                            * (saturated - input);

                    state.phase +=
                        increment;

                    if (state.phase >= 1.0)
                    {
                        state.phase -=
                            std::floor (
                                state.phase);
                        state.held =
                            quantise (
                                driven,
                                effectiveBits,
                                false);
                    }

                    float output =
                        state.held;
                    spEnvelope[
                        (size_t) channel] +=
                        0.002f
                        * (std::abs (output)
                           - spEnvelope[
                               (size_t) channel]);

                    if (spFilterMode
                        != SpFilterMode::Unfiltered)
                    {
                        float cutoff =
                            spFilterCutoffHz;

                        if (spFilterMode
                            == SpFilterMode::Dynamic)
                        {
                            cutoff *=
                                0.40f
                                + 0.60f
                                    * juce::jlimit (
                                        0.0f,
                                        1.0f,
                                        spEnvelope[
                                            (size_t)
                                                channel]
                                            * 3.0f);
                        }

                        const float coefficient =
                            onePoleCoefficient (
                                cutoff,
                                hostRate);
                        float filtered =
                            output;

                        for (auto& stage :
                             state.filter)
                        {
                            stage =
                                coefficient
                                    * stage
                                + (1.0f
                                   - coefficient)
                                    * filtered;
                            filtered = stage;
                        }

                        // The selected output filter reaches its user value at
                        // 100% Scale and disappears at 0%.
                        output +=
                            scale
                            * (filtered - output);
                    }

                    data[sample] =
                        flushDenorm (output);
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

        void processTape (
            juce::AudioBuffer<float>& buffer,
            float amount) noexcept
        {
            const int samples =
                buffer.getNumSamples();
            const int delaySize =
                motionDelay[0].empty()
                    ? 1
                    : (int)
                        motionDelay[0].size();
            const float rate =
                (float) sampleRate;
            const float scale =
                juce::jlimit (
                    0.0f,
                    1.0f,
                    amount);
            const float speed =
                tapeSpeedForMachine();
            const bool cassette =
                tapeMachine
                == TapeMachine::Cassette;

            const float effectiveDrive =
                tapeDrive * scale;
            const float effectiveAge =
                tapeAge * scale;
            const float machineMotion =
                tapeMotion * scale;
            const float effectiveNoise =
                tapeNoise * scale;

            const float wowRate =
                cassette ? 0.55f : 0.22f;
            const float flutterRate =
                cassette ? 8.7f : 5.2f;
            const float wowDepth =
                machineMotion
                * (cassette ? 18.0f : 7.0f)
                * std::sqrt (
                    15.0f / speed);
            const float flutterDepth =
                machineMotion
                * (cassette ? 5.0f : 1.8f)
                * std::sqrt (
                    15.0f / speed);

            const float selectedCutoff =
                cassette
                    ? juce::jmap (
                        effectiveAge,
                        14500.0f,
                        5200.0f)
                        * std::sqrt (
                            speed / 3.75f)
                    : juce::jmap (
                        effectiveAge,
                        20500.0f,
                        9000.0f)
                        * std::sqrt (
                            speed / 15.0f);
            const float lossCoefficient =
                onePoleCoefficient (
                    selectedCutoff,
                    rate);
            const float headBumpFrequency =
                cassette
                    ? 115.0f
                    : 72.0f
                        * (15.0f / speed);
            const float headBumpCoefficient =
                onePoleCoefficient (
                    headBumpFrequency,
                    rate);
            const float driveGain =
                1.0f
                + effectiveDrive
                    * (cassette
                        ? 5.5f
                        : 3.8f);
            const float noiseLevel =
                effectiveNoise
                * (cassette
                    ? 0.018f
                    : 0.006f);

            const bool typeEnabled =
                tapeNoiseReduction
                != TapeNoiseReduction::Off;
            const float legacyDenoise =
                tapeDenoise;
            const float combinedControl =
                juce::jmax (
                    typeEnabled
                        ? tapeNoiseReductionAmount
                        : 0.0f,
                    legacyDenoise);
            const float nrStrengthBase =
                tapeNoiseReduction
                        == TapeNoiseReduction::BStyle
                    ? 0.55f
                    : tapeNoiseReduction
                            == TapeNoiseReduction::CStyle
                        ? 0.82f
                        : 0.0f;
            const float nrStrength =
                nrStrengthBase
                * combinedControl
                * scale;
            const float denoiseStrength =
                combinedControl
                * scale;

            for (int sample = 0;
                 sample < samples;
                 ++sample)
            {
                std::array<float, 2>
                    drySamples {};

                for (int channel = 0;
                     channel < 2;
                     ++channel)
                {
                    const float input =
                        buffer.getSample (
                            channel,
                            sample);
                    drySamples[
                        (size_t) channel] =
                        input;

                    const float nrLow =
                        tapeNrEncodeLow[
                            (size_t) channel]
                            .process (
                                input,
                                onePoleCoefficient (
                                    cassette
                                        ? 1200.0f
                                        : 1800.0f,
                                    rate));
                    const float nrHigh =
                        input - nrLow;

                    tapeNrEnvelope[
                        (size_t) channel] +=
                        0.0015f
                        * (std::abs (nrHigh)
                           - tapeNrEnvelope[
                               (size_t) channel]);
                    const float lowLevelFactor =
                        1.0f
                        - juce::jlimit (
                            0.0f,
                            1.0f,
                            tapeNrEnvelope[
                                (size_t) channel]
                            * 8.0f);
                    const float encoded =
                        input
                        + nrHigh
                            * nrStrength
                            * lowLevelFactor
                            * (cassette
                                ? 2.1f
                                : 1.5f);

                    motionDelay[
                        (size_t) channel]
                               [(size_t)
                                    motionWritePosition] =
                        encoded;
                }

                const float wow =
                    std::sin (wowPhase)
                    + 0.27f
                        * std::sin (
                            wowPhase
                                * 0.43f
                            + 1.3f);
                const float flutter =
                    std::sin (flutterPhase)
                    + 0.28f
                        * std::sin (
                            flutterPhase
                                * 1.89f
                            + 0.4f);

                for (int channel = 0;
                     channel < 2;
                     ++channel)
                {
                    const float stereoOffset =
                        channel == 0
                            ? 0.0f
                            : (cassette
                                ? 0.8f
                                : 0.25f)
                                * machineMotion;
                    const float delaySamples =
                        scale * 48.0f
                        + wow * wowDepth
                        + flutter
                            * flutterDepth
                        + stereoOffset;
                    float tapeInput =
                        scale <= 1.0e-7f
                            ? drySamples[
                                (size_t) channel]
                            : readMotionDelay (
                                channel,
                                juce::jmax (
                                    0.0f,
                                    delaySamples));

                    tapeMemory[
                        (size_t) channel] +=
                        (cassette
                            ? 0.024f
                            : 0.012f)
                        * (tapeInput
                           - tapeMemory[
                               (size_t) channel]);

                    const float magneticInput =
                        tapeInput
                        + tapeMemory[
                            (size_t) channel]
                            * effectiveAge
                            * 0.35f;
                    const float saturated =
                        std::tanh (
                            magneticInput
                            * driveGain)
                        / juce::jmax (
                            0.001f,
                            std::tanh (
                                driveGain));
                    float output =
                        magneticInput
                        + effectiveDrive
                            * (saturated
                               - magneticInput);

                    const float bumpLow =
                        tapeHeadBumpLow[
                            (size_t) channel]
                            .process (
                                output,
                                headBumpCoefficient);
                    const float bumpBand =
                        bumpLow
                        - tapeHeadBumpBand[
                            (size_t) channel]
                            .process (
                                bumpLow,
                                headBumpCoefficient);
                    output +=
                        bumpBand
                        * scale
                        * (cassette
                            ? 0.08f
                            : 0.18f)
                        * (1.0f
                           - speed / 45.0f);

                    const float filtered =
                        tapeLowpass[
                            (size_t) channel]
                            .process (
                                output,
                                lossCoefficient);
                    output +=
                        effectiveAge
                        * (filtered - output);

                    float noise =
                        randomBipolar();
                    tapeNoiseLow[
                        (size_t) channel] +=
                        0.04f
                        * (noise
                           - tapeNoiseLow[
                               (size_t)
                                   channel]);

                    if (cassette)
                    {
                        noise -=
                            tapeNoiseLow[
                                (size_t) channel]
                            * 0.82f;
                    }
                    else
                    {
                        noise =
                            tapeNoiseLow[
                                (size_t) channel]
                                * 0.58f
                            + noise * 0.42f;
                    }

                    output +=
                        noise * noiseLevel;

                    const float decodedLow =
                        tapeNrDecodeLow[
                            (size_t) channel]
                            .process (
                                output,
                                onePoleCoefficient (
                                    cassette
                                        ? 1200.0f
                                        : 1800.0f,
                                    rate));
                    float decodedHigh =
                        output - decodedLow;
                    const float decodeEnvelope =
                        std::abs (
                            decodedHigh);
                    const float decodeLowLevel =
                        1.0f
                        - juce::jlimit (
                            0.0f,
                            1.0f,
                            decodeEnvelope
                            * 8.0f);
                    decodedHigh /=
                        1.0f
                        + nrStrength
                            * decodeLowLevel
                            * (cassette
                                ? 2.1f
                                : 1.5f);
                    output =
                        decodedLow
                        + decodedHigh;

                    tapeDenoiseEnvelope[
                        (size_t) channel] +=
                        0.0008f
                        * (std::abs (
                               decodedHigh)
                           - tapeDenoiseEnvelope[
                               (size_t)
                                   channel]);
                    const float
                        denoiseThreshold =
                            cassette
                                ? 0.018f
                                : 0.007f;
                    const float denoiseGate =
                        juce::jlimit (
                            0.0f,
                            1.0f,
                            tapeDenoiseEnvelope[
                                (size_t) channel]
                            / denoiseThreshold);
                    output =
                        decodedLow
                        + decodedHigh
                            * juce::jmap (
                                denoiseStrength,
                                1.0f,
                                denoiseGate);

                    buffer.setSample (
                        channel,
                        sample,
                        flushDenorm (output));
                }

                motionWritePosition =
                    (motionWritePosition + 1)
                    % delaySize;
                wowPhase +=
                    juce::MathConstants<float>::twoPi
                    * wowRate
                    / rate;
                flutterPhase +=
                    juce::MathConstants<float>::twoPi
                    * flutterRate
                    / rate;

                if (wowPhase
                    >= juce::MathConstants<
                        float>::twoPi)
                {
                    wowPhase -=
                        juce::MathConstants<
                            float>::twoPi;
                }

                if (flutterPhase
                    >= juce::MathConstants<
                        float>::twoPi)
                {
                    flutterPhase -=
                        juce::MathConstants<
                            float>::twoPi;
                }

                uiMotion.store (
                    juce::jlimit (
                        -1.0f,
                        1.0f,
                        scale
                        * (wow * 0.75f
                           + flutter
                               * 0.25f)),
                    std::memory_order_relaxed);
            }
        }

        void processVinyl (
            juce::AudioBuffer<float>& buffer,
            float amount) noexcept
        {
            const float scale =
                juce::jlimit (
                    0.0f,
                    1.0f,
                    amount);
            const float effectiveWear =
                vinylWear * scale;
            const float cutoff =
                juce::jmap (
                    effectiveWear,
                    19000.0f,
                    5500.0f);
            const float coefficient =
                onePoleCoefficient (
                    cutoff,
                    (float) sampleRate);
            const float surfaceLevel =
                vinylSurface
                * scale
                * 0.012f;

            for (int sample = 0;
                 sample < buffer.getNumSamples();
                 ++sample)
            {
                const bool dustEvent =
                    randomUnipolar()
                    < vinylDust
                        * scale
                        * 0.00008f;
                const bool crackleEvent =
                    randomUnipolar()
                    < vinylCrackle
                        * scale
                        * 0.00045f;

                for (int channel = 0;
                     channel < juce::jmin (
                         2,
                         buffer.getNumChannels());
                     ++channel)
                {
                    const float dry =
                        buffer.getSample (
                            channel,
                            sample);
                    const float filtered =
                        vinylLowpass[
                            (size_t) channel]
                            .process (
                                dry,
                                coefficient);
                    float output =
                        dry
                        + effectiveWear
                            * (filtered - dry);

                    float surface =
                        randomBipolar();
                    const float surfaceLow =
                        vinylHighpassLow[
                            (size_t) channel]
                            .process (
                                surface,
                                onePoleCoefficient (
                                    1800.0f,
                                    (float)
                                        sampleRate));
                    surface -=
                        surfaceLow;

                    if (dustEvent)
                    {
                        vinylCrackleEnvelope[
                            (size_t) channel] +=
                            randomBipolar()
                            * 0.25f;
                    }

                    if (crackleEvent)
                    {
                        vinylCrackleEnvelope[
                            (size_t) channel] +=
                            randomBipolar()
                            * 0.65f;
                    }

                    vinylCrackleEnvelope[
                        (size_t) channel] *=
                        0.91f;

                    output +=
                        surface
                            * surfaceLevel
                        + vinylCrackleEnvelope[
                            (size_t) channel]
                            * scale;

                    buffer.setSample (
                        channel,
                        sample,
                        flushDenorm (output));
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

        float lossyRangeHz = 12000.0f;
        float lossyDetail = 0.65f;
        float lossyDamage = 0.35f;
        float lossyScramble = 0.35f;
        float lossyRateHz = 2.0f;
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
