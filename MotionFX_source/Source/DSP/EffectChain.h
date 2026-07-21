#pragma once
#include "Modulation.h"
#include "DriveEffect.h"
#include "PanEffect.h"
#include "VolumeEffect.h"
#include "SpaceEffect.h"
#include "RetroEffect.h"
#include "WidthEffect.h"
#include "StutterEngine.h"
#include <atomic>

namespace mfx
{
    enum class EffectId { Drive = 0, Pan, Volume, Space, Retro, Width };
    static constexpr int numEffects = 6;

    struct EffectSlotState
    {
        bool enabled = true;
        float base01 = 0.5f; // normalised primary (modulatable) parameter
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
            stutter.prepare (sr);
            for (auto& s : slots) s.mod.prepare (sr);

            dryBuffer.setSize (2, maxBlockSize);
            inGainSm.reset (sr, 15.0f, 1.0f);
            outGainSm.reset (sr, 15.0f, 1.0f);
            dryWetSm.reset (sr, 15.0f, 1.0f);
            matchGainSm.reset (sr, 300.0f, 0.0f);
            reset();
        }

        void reset()
        {
            drive.reset(); pan.reset(); volume.reset(); space.reset();
            retro.reset(); width.reset(); stutter.reset();
            for (auto& s : slots) s.mod.reset();
        }

        void processBlock (juce::AudioBuffer<float>& buffer, const TransportInfo& transport) noexcept
        {
            const int n = buffer.getNumSamples();
            if (buffer.getNumChannels() < 2 || n == 0) return;

            auto* L = buffer.getWritePointer (0);
            auto* R = buffer.getWritePointer (1);

            inGainSm.setTarget (dbToGain (inputGainDb));
            for (int i = 0; i < n; ++i)
            {
                float g = inGainSm.next();
                L[i] *= g; R[i] *= g;
            }

            dryBuffer.setSize (2, n, false, false, true);
            dryBuffer.copyFrom (0, 0, buffer, 0, 0, n);
            dryBuffer.copyFrom (1, 0, buffer, 1, 0, n);

            float inputRms = 0.5f * (buffer.getRMSLevel (0, 0, n) + buffer.getRMSLevel (1, 0, n));

            std::array<EffectId, numEffects> currentOrder;
            { const juce::SpinLock::ScopedLockType l (orderLock); currentOrder = order; }

            for (auto id : currentOrder)
            {
                auto idx = (int) id;
                auto& slot = slots[(size_t) idx];
                if (! slot.enabled) continue;

                float modulated = computeParam (slot.base01, slot.mod, transport, n, inputRms);
                uiModValue[(size_t) idx].store (modulated, std::memory_order_relaxed);

                switch (id)
                {
                    case EffectId::Drive:  drive.processBlock (buffer, modulated); break;
                    case EffectId::Pan:    pan.processBlock (buffer, modulated * 2.0f - 1.0f); break;
                    case EffectId::Volume: volume.processBlock (buffer, modulated); break;
                    case EffectId::Space:  space.processBlock (buffer, modulated); break;
                    case EffectId::Retro:  retro.processBlock (buffer, modulated); break;
                    case EffectId::Width:  width.processBlock (buffer, modulated); break;
                }
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
            for (int i = 0; i < n; ++i)
            {
                float m = dryWetSm.next();
                L[i] = dryBuffer.getSample (0, i) * (1.0f - m) + L[i] * m;
                R[i] = dryBuffer.getSample (1, i) * (1.0f - m) + R[i] * m;
            }

            float outputRms = 0.5f * (buffer.getRMSLevel (0, 0, n) + buffer.getRMSLevel (1, 0, n));
            float matchDb = 0.0f;
            if (matchGainEnabled && outputRms > 1.0e-6f && inputRms > 1.0e-6f)
                matchDb = juce::jlimit (-24.0f, 24.0f, gainToDb (inputRms) - gainToDb (outputRms));
            matchGainSm.setTarget (matchDb);

            outGainSm.setTarget (dbToGain (outputGainDb));
            for (int i = 0; i < n; ++i)
            {
                float mg = dbToGain (matchGainSm.next());
                float og = outGainSm.next();
                L[i] = flushDenorm (safetyCeiling (L[i] * og * mg));
                R[i] = flushDenorm (safetyCeiling (R[i] * og * mg));
            }

            outputLevelUi.store (buffer.getMagnitude (0, n), std::memory_order_relaxed);
            sanitizeBuffer (buffer);
        }

        // --- shared config, written by the processor from APVTS each block ---
        std::array<EffectId, numEffects> order { EffectId::Drive, EffectId::Retro, EffectId::Pan,
                                                  EffectId::Width, EffectId::Volume, EffectId::Space };
        mutable juce::SpinLock orderLock;
        std::array<EffectId, numEffects> getOrderSafe() const noexcept
        {
            const juce::SpinLock::ScopedLockType l (orderLock);
            return order;
        }
        void setOrderSafe (std::array<EffectId, numEffects> o) noexcept
        {
            const juce::SpinLock::ScopedLockType l (orderLock);
            order = o;
        }

        std::array<EffectSlotState, numEffects> slots;

        DriveEffect drive; PanEffect pan; VolumeEffect volume;
        SpaceEffect space; RetroEffect retro; WidthEffect width;
        StutterEngine stutter;

        float inputGainDb = 0.0f, outputGainDb = 0.0f, dryWet = 1.0f;
        bool matchGainEnabled = false;
        bool stutterEnabled = false;

        // --- live values for the GUI (lock-free reads) ---
        std::array<std::atomic<float>, numEffects> uiModValue {};
        std::atomic<float> outputLevelUi { 0.0f };

    private:
        static float computeParam (float base01, ModulationUnit& mod, const TransportInfo& t, int n, float sidechain) noexcept
        {
            if (mod.source == ModSourceType::Off || mod.depth <= 0.0001f)
                return base01;
            float modVal = mod.getValue (t, n, sidechain);
            return juce::jlimit (0.0f, 1.0f, base01 * (1.0f - mod.depth) + modVal * mod.depth);
        }

        double sampleRate = 44100.0;
        juce::AudioBuffer<float> dryBuffer;
        Smoothed inGainSm, outGainSm, dryWetSm, matchGainSm;
    };
}
