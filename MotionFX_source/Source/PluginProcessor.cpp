#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace mfx;

MotionFXAudioProcessor::MotionFXAudioProcessor()
    : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    presetManager.attach (&apvts,
        [this] { return getOrder(); },
        [this] (std::array<EffectId, numEffects> o) { setOrder (o); });

    stateHistory = std::make_unique<mfx::StateHistory> (
        apvts,
        *this,
        [this] { return presetManager.getFullStateXml(); },
        [this] (const juce::String& xml)
        {
            presetManager.restoreFullStateXml (xml);
        });
}

MotionFXAudioProcessor::~MotionFXAudioProcessor()
{
    stateHistory.reset();
}

void MotionFXAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    setRateAndBufferSizeDetails (sampleRate, samplesPerBlock);
    chain.prepare (sampleRate, samplesPerBlock);
    fallbackPpq = 0.0;
}

void MotionFXAudioProcessor::releaseResources() {}

bool MotionFXAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

static mfx::SyncDiv toDiv (int idx) noexcept
{
    return (mfx::SyncDiv) juce::jlimit (0, mfx::syncDivCount - 1, idx);
}

static float freeRateToHz (float value, int unit) noexcept
{
    // Unit 0 = cycles per second, unit 1 = seconds per cycle.
    return unit == 1 ? 1.0f / juce::jmax (0.01f, value) : value;
}

static float spaceDelayRateHzFromPercent (float percent) noexcept
{
    const float normalised = juce::jlimit (0.0f, 100.0f, percent) / 100.0f;
    return 0.1f * std::pow (200.0f, normalised);
}

void MotionFXAudioProcessor::updateSlot (mfx::EffectId id, const juce::String& prefix, int numSamples)
{
    juce::ignoreUnused (numSamples);
    auto raw = [this, &prefix] (const char* name) -> float
    {
        auto* v = apvts.getRawParameterValue (prefix + "_" + name);
        return v != nullptr ? v->load() : 0.0f;
    };

    auto& slot = chain.slots[(size_t) id];
    slot.enabled = raw ("enabled") > 0.5f;
    slot.base01 = raw ("base")
        / (id == EffectId::Width ? 200.0f : 100.0f);

    auto& mod = slot.mod;
    mod.source = (mfx::ModSourceType) (int) raw ("modsource");
    mod.depth = raw ("moddepth") / 100.0f;

    mod.lfo.setParams ((mfx::LfoShape) (int) raw ("lfo_shape"), raw ("lfo_synced") > 0.5f,
                        freeRateToHz (raw ("lfo_rate"), (int) raw ("lfo_rateunit")),
                        toDiv ((int) raw ("lfo_div")), false);

    switch ((int) raw ("motion_shape"))
    {
        case 0: mod.motion.setPluckThenRise(); break;
        case 1: mod.motion.setRiseThenFall(); break;
        case 2: mod.motion.setRamp(); break;
        case 3: mod.motion.setDip(); break;
        default: mod.motion.setFlat(); break;
    }
    mod.motion.setParams ((mfx::MotionMode) (int) raw ("motion_mode"), raw ("motion_synced") > 0.5f,
                           freeRateToHz (raw ("motion_rate"), (int) raw ("motion_rateunit")),
                           toDiv ((int) raw ("motion_div")), raw ("motion_smooth") / 100.0f);

    mod.env.setTimes (raw ("env_attack"), raw ("env_release"));

    int numSteps = (int) raw ("seq_numsteps");
    mod.seq.setNumSteps (numSteps);
    mod.seq.setParams (raw ("seq_synced") > 0.5f,
                       freeRateToHz (raw ("seq_rate"), (int) raw ("seq_rateunit")),
                       toDiv ((int) raw ("seq_div")), raw ("seq_smooth"));
    for (int i = 0; i < numSteps; ++i)
    {
        auto* v = apvts.getRawParameterValue (prefix + "_seq_step" + juce::String (i));
        mod.seq.setStep (i, (v != nullptr ? v->load() : 100.0f) / 100.0f);
    }

    switch (id)
    {
        case EffectId::Drive:
            chain.drive.setMode ((mfx::DriveMode) (int) raw ("mode"));
            chain.drive.setParams (
                raw ("tone") / 100.0f,
                raw ("mix") / 100.0f,
                raw ("outtrim"),
                raw ("bias") / 100.0f,
                (int) raw ("quality"),
                (int) raw ("postclip"));

            chain.drive.setGroovePhaseParams (
                raw ("trace_enabled") > 0.5f,
                raw ("trace_gain"),
                raw ("trace_freq"),
                raw ("trace_bandwidth"),
                raw ("pinch_enabled") > 0.5f,
                raw ("pinch_gain"),
                raw ("pinch_freq"),
                raw ("pinch_bandwidth"),
                (int) raw ("groove_character"),
                raw ("pinch_stereo") > 0.5f);
            break;
        case EffectId::Pan:
            chain.pan.setMode ((mfx::PanMode) (int) raw ("mode"));
            chain.pan.setParams (raw ("widthinfluence") / 100.0f);
            break;
        case EffectId::Volume:
            chain.volume.setMode ((mfx::VolumeMode) (int) raw ("mode"));
            break;
        case EffectId::Space:
        {
            const int modeIndex = (int) raw ("mode");
            chain.space.setMode ((mfx::SpaceMode) modeIndex);
            chain.space.setParams (raw ("size") / 100.0f,
                                   raw ("decay") / 100.0f,
                                   raw ("tone") / 100.0f);

            const bool delayMode = modeIndex == 2 || modeIndex == 3 || modeIndex == 5;
            if (delayMode)
            {
                float delaySeconds = 1.0f / spaceDelayRateHzFromPercent (raw ("size"));
                if (raw ("delay_synced") > 0.5f)
                {
                    const double beats = mfx::syncDivToBeats (toDiv ((int) raw ("delay_div")));
                    delaySeconds = (float) (beats * 60.0 / juce::jmax (1.0, currentTransport.bpm));
                }
                chain.space.setDelayTimeSeconds (delaySeconds);
            }
            break;
        }
        case EffectId::Retro:
            chain.retro.setMode ((mfx::RetroMode) (int) raw ("mode"));
            chain.retro.setParams (raw ("rate") / 100.0f, raw ("tone") / 100.0f, raw ("mix") / 100.0f);
            break;
        case EffectId::Width:
            chain.width.setMode ((mfx::WidthMode) (int) raw ("mode"));
            chain.width.setParams (raw ("crossover") / 100.0f);
            break;
        case EffectId::Filter:
            chain.filter.setMode ((mfx::FilterMode) (int) raw ("mode"));
            chain.filter.setParams (raw ("resonance") / 100.0f,
                                    (int) raw ("slope"),
                                    raw ("mix") / 100.0f);
            break;
    }
}

void MotionFXAudioProcessor::pullParamsIntoChain (int numSamples)
{
    auto rawM = [this] (const juce::String& id) -> float
    {
        auto* value = apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    };

    chain.inputGainDb = rawM ("master_input");
    chain.outputGainDb = rawM ("master_output");
    chain.dryWet = rawM ("master_drywet") / 100.0f;
    chain.matchGainEnabled = rawM ("master_matchgain") > 0.5f;

    for (int e = 0; e < numEffects; ++e)
        updateSlot ((EffectId) e, effectPrefixes[e], numSamples);

    chain.stutterEnabled = rawM ("stutter_enabled") > 0.5f;
    const int stutterSteps = (int) rawM ("stutter_numsteps");
    chain.stutter.setPattern (stutterSteps, toDiv ((int) rawM ("stutter_div")));
    chain.stutter.setMix (rawM ("stutter_mix") / 100.0f);
    chain.stutter.setPitchGrainMilliseconds (
        rawM ("stutter_pitch_grain_ms"));

    for (int step = 0; step < mfx::StutterEngine::maxSteps; ++step)
    {
        const juce::String index (step);
        chain.stutter.setRepeatStep (
            step, (mfx::RepeatAction) (int) rawM ("stutter_repeat_step" + index));
        chain.stutter.setReverseStep (
            step, rawM ("stutter_reverse_step" + index) > 0.5f);
        chain.stutter.setTapeStep (
            step, (mfx::TapeAction) (int) rawM ("stutter_tape_step" + index));
        chain.stutter.setPitchStep (
            step,
            rawM ("stutter_pitch_step" + index) > 0.5f,
            (int) rawM ("stutter_pitch_semitones_step" + index));
        chain.stutter.setGateStep (
            step, rawM ("stutter_gate_step" + index) > 0.5f);
    }
}

void MotionFXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    double bpm = 120.0, ppq = 0.0;
    bool isPlaying = false, gotPpq = false, hostTransportAvailable = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            hostTransportAvailable = true;
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) { ppq = *p; gotPpq = true; }
            isPlaying = pos->getIsPlaying();
        }
    }

    currentTransport.bpm = bpm;
    currentTransport.isPlaying = isPlaying;

    // A host may keep calling processBlock while its transport is stopped.
    // In that state MotionFX must behave as a paused processor: no LFO/step
    // advancement, no stutter capture and no delay/reverb/noise tail continuing
    // on its own. The incoming buffer is therefore left dry and untouched.
    // When no usable playhead exists (standalone/headless tests), processing
    // remains enabled so hosts that do not expose transport data still work.
    if (hostTransportAvailable && ! isPlaying)
    {
        if (wasHostPlaying)
            chain.reset();

        wasHostPlaying = false;
        currentTransport.ppqPosition = gotPpq ? ppq : fallbackPpq;
        return;
    }

    wasHostPlaying = isPlaying;

    if (gotPpq)
    {
        currentTransport.ppqPosition = ppq;
        fallbackPpq = ppq;
    }
    else
    {
        const double sr = getSampleRate();
        if (sr > 0.0)
            fallbackPpq += ((double) numSamples / sr) * (bpm / 60.0);
        currentTransport.ppqPosition = fallbackPpq;
    }

    pullParamsIntoChain (numSamples);

    const int requiredLatency =
        chain.getLatencySamples();

    if (getLatencySamples() != requiredLatency)
        setLatencySamples (requiredLatency);

    if (buffer.getNumChannels() >= 2)
        chain.processBlock (buffer, currentTransport);
}

juce::AudioProcessorEditor* MotionFXAudioProcessor::createEditor()
{
    return new MotionFXAudioProcessorEditor (*this);
}

void MotionFXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xmlString = presetManager.getFullStateXml();
    juce::MemoryOutputStream mos (destData, false);
    mos.writeString (xmlString);
}

void MotionFXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mis (data, (size_t) sizeInBytes, false);
    auto xmlString = mis.readString();
    if (xmlString.isNotEmpty())
        presetManager.restoreFullStateXml (xmlString);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MotionFXAudioProcessor();
}
