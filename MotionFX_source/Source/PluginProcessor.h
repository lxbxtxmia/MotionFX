#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "DSP/EffectChain.h"
#include "PresetManager.h"
#include "StateHistory.h"

class MotionFXAudioProcessor : public juce::AudioProcessor
{
public:
    MotionFXAudioProcessor();
    ~MotionFXAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MotionFX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts {
        *this, nullptr, "PARAMS", mfx::createParameterLayout()
    };
    mfx::EffectChain chain;
    mfx::PresetManager presetManager;
    std::unique_ptr<mfx::StateHistory> stateHistory;

    std::array<mfx::EffectId, mfx::numEffects> getOrder() const
    {
        return chain.getOrderSafe();
    }

    void setOrder (std::array<mfx::EffectId, mfx::numEffects> order)
    {
        chain.setOrderSafe (order);
    }

private:
    void pullParamsIntoChain (int numSamples);
    void updateSlot (mfx::EffectId id,
                     const juce::String& prefix,
                     int numSamples);

    mfx::TransportInfo currentTransport;
    double fallbackPpq = 0.0;
    bool wasHostPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MotionFXAudioProcessor)
};
