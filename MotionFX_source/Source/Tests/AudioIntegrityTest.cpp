#include "../PluginProcessor.h"
#include "../FactoryPresets.h"
#include <iostream>
#include <cmath>

static int failures = 0;
static int totalBlocksChecked = 0;
static int silentBlocksThisPass = 0;

static void checkBuffer (const juce::AudioBuffer<float>& buf, const juce::String& context, bool inputWasSilent)
{
    juce::ignoreUnused (context);
    bool hasNaNInf = false;
    float maxAbs = 0.0f;
    double sumSq = 0.0;
    int n = buf.getNumSamples();

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const auto* d = buf.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
        {
            float v = d[i];
            if (! std::isfinite (v)) { hasNaNInf = true; }
            maxAbs = juce::jmax (maxAbs, std::abs (v));
            sumSq += (double) v * v;
        }
    }
    float rms = (float) std::sqrt (sumSq / juce::jmax (1, n * buf.getNumChannels()));

    ++totalBlocksChecked;
    if (hasNaNInf)
    {
        std::cout << "  [FAIL] NaN/Inf detected -- " << context << std::endl;
        ++failures;
    }
    if (maxAbs > 8.0f)
    {
        std::cout << "  [FAIL] Runaway output level (peak " << maxAbs << ") -- " << context << std::endl;
        ++failures;
    }
    if (! inputWasSilent && rms < 1.0e-7f)
        ++silentBlocksThisPass;
}

static void fillSineSweep (juce::AudioBuffer<float>& buf, double sampleRate, double& phase, double& freq)
{
    int n = buf.getNumSamples();
    for (int i = 0; i < n; ++i)
    {
        float s = 0.4f * (float) std::sin (phase);
        buf.setSample (0, i, s);
        buf.setSample (1, i, s);
        phase += juce::MathConstants<double>::twoPi * freq / sampleRate;
        if (phase > juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
        freq += (20000.0 - 20.0) / (sampleRate * 4.0); // sweep 20Hz-20kHz over ~4 seconds
        if (freq > 20000.0) freq = 20.0;
    }
}

static void fillNoise (juce::AudioBuffer<float>& buf, juce::Random& rng)
{
    int n = buf.getNumSamples();
    for (int i = 0; i < n; ++i)
    {
        float s = rng.nextFloat() * 2.0f - 1.0f;
        buf.setSample (0, i, s * 0.5f);
        buf.setSample (1, i, s * 0.5f);
    }
}

static void runPass (MotionFXAudioProcessor& proc, double sr, int blockSize, const juce::String& label)
{
    std::cout << "-- " << label << " @ " << sr << "Hz / block " << blockSize << std::endl;
    proc.prepareToPlay (sr, blockSize);

    juce::AudioBuffer<float> buf (2, blockSize);
    juce::MidiBuffer midi;
    double phase = 0.0, freq = 20.0;
    juce::Random rng (42);
    silentBlocksThisPass = 0;
    int nonSilentInputBlocks = 0;

    int blocksFor4s = (int) (sr * 4.0 / blockSize) + 1;
    for (int b = 0; b < blocksFor4s; ++b)
    {
        buf.clear();
        fillSineSweep (buf, sr, phase, freq);
        proc.processBlock (buf, midi);
        checkBuffer (buf, label + " sine sweep block " + juce::String (b), false);
        ++nonSilentInputBlocks;
    }

    int blocksFor2s = (int) (sr * 2.0 / blockSize) + 1;
    for (int b = 0; b < blocksFor2s; ++b)
    {
        buf.clear();
        fillNoise (buf, rng);
        proc.processBlock (buf, midi);
        checkBuffer (buf, label + " noise block " + juce::String (b), false);
        ++nonSilentInputBlocks;
    }

    int blocksFor1s = (int) (sr * 1.0 / blockSize) + 1;
    for (int b = 0; b < blocksFor1s; ++b)
    {
        buf.clear();
        proc.processBlock (buf, midi);
        checkBuffer (buf, label + " silence block " + juce::String (b), true);
    }

    if (nonSilentInputBlocks > 0)
    {
        float pct = 100.0f * (float) silentBlocksThisPass / (float) nonSilentInputBlocks;
        if (pct > 95.0f)
        {
            std::cout << "  [FAIL] " << pct << "% of non-silent-input blocks produced silence -- likely dead signal path" << std::endl;
            ++failures;
        }
        else if (pct > 0.0f)
        {
            std::cout << "  (" << pct << "% silent blocks -- expected if this preset uses Gate/Off stutter steps)" << std::endl;
        }
    }

    proc.releaseResources();
}

int main()
{
    std::cout << "=== MotionFX audio integrity test ===" << std::endl;

    MotionFXAudioProcessor proc;
    int numFactory = proc.presetManager.getNumFactoryPresets();
    std::cout << "Factory presets found: " << numFactory << " (expect 16)" << std::endl;
    if (numFactory != 16) { std::cout << "  [FAIL] preset count mismatch" << std::endl; ++failures; }



    // 1) default state, a few sample rates / block sizes
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int bs : { 64, 512, 2048 })
            runPass (proc, sr, bs, "Default state");

    // 2) every factory preset, at 48kHz/512 (representative host settings)
    for (int i = 0; i < numFactory; ++i)
    {
        proc.presetManager.loadFactoryPreset (i);
        juce::String name = proc.presetManager.getFactoryPresetName (i);
        std::cout << "Preset " << (i + 1) << "/" << numFactory << ": " << name << std::endl;

        if (proc.presetManager.getCurrentName() != name)
        {
            std::cout << "  [FAIL] preset name mismatch after load: expected '" << name
                       << "' got '" << proc.presetManager.getCurrentName() << "'" << std::endl;
            ++failures;
        }
        runPass (proc, 48000.0, 512, "Preset: " + name);
    }

    // 3) extreme parameter stress test -- push every effect to max drive/feedback/depth at once
    std::cout << "-- Extreme parameter stress test" << std::endl;
    for (auto* id : { "drive", "pan", "volume", "space", "retro", "width" })
    {
        juce::String prefix (id);
        if (auto* p = proc.apvts.getParameter (prefix + "_enabled")) p->setValueNotifyingHost (1.0f);
        if (auto* p = proc.apvts.getParameter (prefix + "_base")) p->setValueNotifyingHost (1.0f);
        if (auto* p = proc.apvts.getParameter (prefix + "_modsource")) p->setValueNotifyingHost (p->convertTo0to1 (4.0f)); // Sequencer
        if (auto* p = proc.apvts.getParameter (prefix + "_moddepth")) p->setValueNotifyingHost (1.0f);
    }
    if (auto* p = proc.apvts.getParameter ("space_decay")) p->setValueNotifyingHost (1.0f); // max feedback
    if (auto* p = proc.apvts.getParameter ("stutter_enabled")) p->setValueNotifyingHost (1.0f);
    if (auto* p = proc.apvts.getParameter ("stutter_mix")) p->setValueNotifyingHost (1.0f);
    for (int i = 0; i < 16; ++i)
        if (auto* p = proc.apvts.getParameter ("stutter_step" + juce::String (i)))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) (1 + (i % 10))));
    runPass (proc, 44100.0, 128, "Extreme stress");

    std::cout << "=== RESULT: " << failures << " failing checks across " << totalBlocksChecked << " blocks tested ===" << std::endl;
    return failures > 0 ? 1 : 0;
}
