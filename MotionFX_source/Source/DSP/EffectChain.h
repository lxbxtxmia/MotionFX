#pragma once
#include "Modulation.h"
#include "DriveEffect.h"
#include "PanEffect.h"
#include "VolumeEffect.h"
#include "SpaceEffect.h"
#include "RetroEffect.h"
#include "WidthEffect.h"
#include "FilterEffect.h"
#include "StutterEngine.h"
#include <atomic>

namespace mfx
{
    enum class EffectId { Drive = 0, Pan, Volume, Space, Retro, Width, Filter };
    static constexpr int numEffects = 7;

    struct EffectSlotState
    {
        bool enabled = false;
        float base01 = 0.5f;
        ModulationUnit mod;
    };

    class EffectChain
    {
    public:
        void prepare (double sr, int maxBlockSize)
        {
            sampleRate = sr;
            drive.prepare (sr);
            pan.prepare (sr);
            volume.prepare (sr);
            space.prepare (sr, maxBlockSize);
            retro.prepare (sr);
            width.prepare (sr);
            filter.prepare (sr, maxBlockSize);
            stutter.prepare (sr);

            for (auto& slot : slots)
                slot.mod.prepare (sr);

            dryBuffer.setSize (2, maxBlockSize);
            inGainSm.reset (sr, 15.0f, 1.0f);
            outGainSm.reset (sr, 15.0f, 1.0f);
            dryWetSm.reset (sr, 15.0f, 1.0f);
            matchGainSm.reset (sr, 300.0f, 0.0f);
            reset();
        }

        void reset()
        {
            drive.reset();
            pan.reset();
            volume.reset();
            space.reset();
            retro.reset();
            width.reset();
            filter.reset();
            stutter.reset();

            for (auto& slot : slots)
                slot.mod.reset();

            for (auto& level : uiInputLevel) level.store (0.0f, std::memory_order_relaxed);
            for (auto& level : uiOutputLevel) level.store (0.0f, std::memory_order_relaxed);
            for (auto& value : uiModValue) value.store (0.0f, std::memory_order_relaxed);
            outputLevelUi.store (0.0f, std::memory_order_relaxed);
            outputLeftLevelUi.store (0.0f, std::memory_order_relaxed);
            outputRightLevelUi.store (0.0f, std::memory_order_relaxed);
            uiSignalEpoch.fetch_add (1, std::memory_order_relaxed);
        }

        void processBlock (juce::AudioBuffer<float>& buffer,
                           const TransportInfo& transport) noexcept
        {
            const int numSamples = buffer.getNumSamples();
            if (buffer.getNumChannels() < 2 || numSamples == 0)
                return;

            uiSignalEpoch.fetch_add (1, std::memory_order_relaxed);

            auto* left = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);

            inGainSm.setTarget (dbToGain (inputGainDb));
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float gain = inGainSm.next();
                left[sample] *= gain;
                right[sample] *= gain;
            }

            dryBuffer.setSize (2, numSamples, false, false, true);
            dryBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
            dryBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);

            const float inputRms = 0.5f * (buffer.getRMSLevel (0, 0, numSamples)
                                         + buffer.getRMSLevel (1, 0, numSamples));

            std::array<EffectId, numEffects> currentOrder;
            {
                const juce::SpinLock::ScopedLockType lock (orderLock);
                currentOrder = order;
            }

            for (auto effect : currentOrder)
            {
                const int index = (int) effect;
                auto& slot = slots[(size_t) index];

                const float preLevel = stageMagnitude (buffer);
                uiInputLevel[(size_t) index].store (preLevel, std::memory_order_relaxed);

                if (! slot.enabled)
                {
                    uiModValue[(size_t) index].store (slot.base01, std::memory_order_relaxed);
                    uiOutputLevel[(size_t) index].store (preLevel, std::memory_order_relaxed);
                    continue;
                }

                const float modulated = computeParam (slot.base01, slot.mod,
                                                      transport, numSamples, inputRms);
                uiModValue[(size_t) index].store (modulated, std::memory_order_relaxed);

                switch (effect)
                {
                    case EffectId::Drive:  drive.processBlock (buffer, modulated); break;
                    case EffectId::Pan:    pan.processBlock (buffer, modulated * 2.0f - 1.0f); break;
                    case EffectId::Volume: volume.processBlock (buffer, modulated); break;
                    case EffectId::Space:  space.processBlock (buffer, modulated); break;
                    case EffectId::Retro:  retro.processBlock (buffer, modulated); break;
                    case EffectId::Width:  width.processBlock (buffer, modulated); break;
                    case EffectId::Filter: filter.processBlock (buffer, modulated); break;
                }

                uiOutputLevel[(size_t) index].store (stageMagnitude (buffer),
                                                     std::memory_order_relaxed);
            }

            if (stutterEnabled)
            {
                stutter.setEnabled (true);
                stutter.processBlock (buffer, transport);
            }
            else
            {
                stutter.setEnabled (false);
            }

            dryWetSm.setTarget (dryWet);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float amount = dryWetSm.next();
                left[sample] = dryBuffer.getSample (0, sample) * (1.0f - amount)
                             + left[sample] * amount;
                right[sample] = dryBuffer.getSample (1, sample) * (1.0f - amount)
                              + right[sample] * amount;
            }

            const float outputRms = 0.5f * (buffer.getRMSLevel (0, 0, numSamples)
                                          + buffer.getRMSLevel (1, 0, numSamples));
            float matchDb = 0.0f;
            if (matchGainEnabled && outputRms > 1.0e-6f && inputRms > 1.0e-6f)
                matchDb = juce::jlimit (-24.0f, 24.0f,
                                        gainToDb (inputRms) - gainToDb (outputRms));
            matchGainSm.setTarget (matchDb);

            outGainSm.setTarget (dbToGain (outputGainDb));
            float rawPeakLeft = 0.0f;
            float rawPeakRight = 0.0f;

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float matchedGain = dbToGain (matchGainSm.next());
                const float outputGain = outGainSm.next();
                const float rawLeft = left[sample] * outputGain * matchedGain;
                const float rawRight = right[sample] * outputGain * matchedGain;

                rawPeakLeft = juce::jmax (rawPeakLeft, std::abs (rawLeft));
                rawPeakRight = juce::jmax (rawPeakRight, std::abs (rawRight));

                left[sample] = flushDenorm (safetyCeiling (rawLeft));
                right[sample] = flushDenorm (safetyCeiling (rawRight));
            }

            sanitizeBuffer (buffer);
            outputLeftLevelUi.store (rawPeakLeft, std::memory_order_relaxed);
            outputRightLevelUi.store (rawPeakRight, std::memory_order_relaxed);
            outputLevelUi.store (juce::jmax (rawPeakLeft, rawPeakRight),
                                 std::memory_order_relaxed);
        }

        std::array<EffectId, numEffects> order {
            EffectId::Drive, EffectId::Retro, EffectId::Filter, EffectId::Pan,
            EffectId::Width, EffectId::Volume, EffectId::Space
        };

        mutable juce::SpinLock orderLock;

        std::array<EffectId, numEffects> getOrderSafe() const noexcept
        {
            const juce::SpinLock::ScopedLockType lock (orderLock);
            return order;
        }

        void setOrderSafe (std::array<EffectId, numEffects> newOrder) noexcept
        {
            const juce::SpinLock::ScopedLockType lock (orderLock);
            order = newOrder;
        }

        std::array<EffectSlotState, numEffects> slots;

        DriveEffect drive;
        PanEffect pan;
        VolumeEffect volume;
        SpaceEffect space;
        RetroEffect retro;
        WidthEffect width;
        FilterEffect filter;
        StutterEngine stutter;

        float inputGainDb = 0.0f;
        float outputGainDb = 0.0f;
        float dryWet = 1.0f;
        bool matchGainEnabled = false;
        bool stutterEnabled = false;

        std::array<std::atomic<float>, numEffects> uiModValue {};
        std::array<std::atomic<float>, numEffects> uiInputLevel {};
        std::array<std::atomic<float>, numEffects> uiOutputLevel {};
        std::atomic<juce::uint64> uiSignalEpoch { 0 };
        std::atomic<float> outputLevelUi { 0.0f };
        std::atomic<float> outputLeftLevelUi { 0.0f };
        std::atomic<float> outputRightLevelUi { 0.0f };

    private:
        static float stageMagnitude (const juce::AudioBuffer<float>& buffer) noexcept
        {
            if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
                return 0.0f;

            return 0.5f * (buffer.getMagnitude (0, 0, buffer.getNumSamples())
                         + buffer.getMagnitude (1, 0, buffer.getNumSamples()));
        }

        static float computeParam (float base01, ModulationUnit& mod,
                                   const TransportInfo& transport,
                                   int numSamples, float sidechain) noexcept
        {
            if (mod.source == ModSourceType::Off || mod.depth <= 0.0001f)
                return base01;

            const float modulated = mod.getValue (transport, numSamples, sidechain);
            return juce::jlimit (0.0f, 1.0f,
                                 base01 * (1.0f - mod.depth) + modulated * mod.depth);
        }

        double sampleRate = 44100.0;
        juce::AudioBuffer<float> dryBuffer;
        Smoothed inGainSm, outGainSm, dryWetSm, matchGainSm;
    };
}
