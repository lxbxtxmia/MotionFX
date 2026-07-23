#include "../PluginProcessor.h"
#include "../FactoryPresets.h"
#include "../GUI/ValueFormatting.h"
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

    // Block 0.8.1 compact display and parameter-range checks.
    if (mfx::ValueFormatting::frequencyHz (20000.0, false)
            != "20.0 kHz"
        || mfx::ValueFormatting::frequencyHz (100.0, false)
            != "100 Hz"
        || mfx::ValueFormatting::pan (50.0, false) != "C"
        || mfx::ValueFormatting::pan (0.0, false) != "50L"
        || mfx::ValueFormatting::pan (100.0, false) != "50R")
    {
        std::cout << "  [FAIL] Block 0.8.1 compact value formatting is incorrect"
                  << std::endl;
        ++failures;
    }

    if (auto* width = proc.apvts.getParameter ("width_base"))
    {
        if (std::abs (width->convertFrom0to1 (1.0f) - 200.0f) > 0.001f
            || std::abs (width->convertFrom0to1 (0.5f) - 100.0f) > 0.001f)
        {
            std::cout << "  [FAIL] Width parameter is not mapped from 0 to 200%"
                      << std::endl;
            ++failures;
        }
    }
    else
    {
        std::cout << "  [FAIL] Width base parameter is missing" << std::endl;
        ++failures;
    }

    float modulationMinimum = 0.0f;
    float modulationMaximum = 0.0f;
    mfx::ValueFormatting::modulationRange (
        0.5f, 0.4f, modulationMinimum, modulationMaximum);

    if (std::abs (modulationMinimum - 0.3f) > 0.0001f
        || std::abs (modulationMaximum - 0.7f) > 0.0001f)
    {
        std::cout << "  [FAIL] Modulation range display calculation is incorrect"
                  << std::endl;
        ++failures;
    }

    if (std::abs (mfx::RetroEffect::lossyBandwidthHz (1.0f) - 18000.0f) > 0.01f
        || std::abs (mfx::RetroEffect::wearRateHz (1.0f) - 4.6f) > 0.001f
        || std::abs (mfx::RetroEffect::emuFilterHz (0.0f) - 3000.0f) > 0.01f)
    {
        std::cout << "  [FAIL] Retro contextual units do not match DSP mappings"
                  << std::endl;
        ++failures;
    }



    // Block 4 timing choices and absolute Stutter repeat lengths.
    if (mfx::syncDivChoices().size() != mfx::syncDivCount)
    {
        std::cout << "  [FAIL] sync division UI/DSP count mismatch" << std::endl;
        ++failures;
    }

    const auto nearlyEqual = [] (double a, double b) { return std::abs (a - b) < 1.0e-6; };
    if (! nearlyEqual (mfx::syncDivToBeats (mfx::SyncDiv::d1_4D), 1.5)
        || ! nearlyEqual (mfx::syncDivToBeats (mfx::SyncDiv::d1_4T), 2.0 / 3.0)
        || ! nearlyEqual (mfx::syncDivToBeats (mfx::SyncDiv::d1_16D), 0.375))
    {
        std::cout << "  [FAIL] dotted/triplet sync conversion mismatch" << std::endl;
        ++failures;
    }

    for (auto* id : { "drive", "pan", "volume", "space", "retro", "width", "filter" })
    {
        const juce::String prefix (id);
        for (auto* suffix : { "lfo_rateunit", "motion_rateunit", "seq_rateunit" })
        {
            if (proc.apvts.getParameter (prefix + "_" + suffix) == nullptr)
            {
                std::cout << "  [FAIL] missing timing unit parameter " << prefix << "_" << suffix << std::endl;
                ++failures;
            }
        }
    }

    mfx::StutterEngine stutterTimingTest;
    stutterTimingTest.prepare (48000.0);
    if (stutterTimingTest.getNominalRepeatLengthSamples (mfx::RepeatAction::Quarter, 120.0) != 24000
        || stutterTimingTest.getNominalRepeatLengthSamples (mfx::RepeatAction::Eighth, 120.0) != 12000
        || stutterTimingTest.getNominalRepeatLengthSamples (mfx::RepeatAction::Sixteenth, 120.0) != 6000
        || stutterTimingTest.getNominalRepeatLengthSamples (mfx::RepeatAction::ThirtySecond, 120.0) != 3000)
    {
        std::cout << "  [FAIL] Stutter repeat lengths are not absolute musical divisions" << std::endl;
        ++failures;
    }

    if (! nearlyEqual (mfx::StutterEngine::pitchRatioForSemitones (12), 2.0)
        || ! nearlyEqual (mfx::StutterEngine::pitchRatioForSemitones (-12), 0.5)
        || ! nearlyEqual (mfx::StutterEngine::pitchRatioForSemitones (0), 1.0))
    {
        std::cout << "  [FAIL] realtime pitch semitone ratios are incorrect" << std::endl;
        ++failures;
    }

    stutterTimingTest.setPitchStep (0, true, 12);
    stutterTimingTest.setPitchStep (1, true, -7);
    stutterTimingTest.setPitchStep (2, false, 24);
    if (! stutterTimingTest.isPitchStepActive (0)
        || ! stutterTimingTest.isPitchStepActive (1)
        || stutterTimingTest.isPitchStepActive (2)
        || stutterTimingTest.getPitchSemitonesForStep (0) != 12
        || stutterTimingTest.getPitchSemitonesForStep (1) != -7
        || stutterTimingTest.getPitchSemitonesForStep (2) != 24)
    {
        std::cout << "  [FAIL] per-step Pitch values are not independent" << std::endl;
        ++failures;
    }

    for (auto* id : { "stutter_repeat_step0", "stutter_reverse_step0",
                      "stutter_tape_step0", "stutter_pitch_step0",
                      "stutter_pitch_semitones_step0", "stutter_gate_step0",
                      "stutter_pitch_grain_ms" })
    {
        if (proc.apvts.getParameter (id) == nullptr)
        {
            std::cout << "  [FAIL] missing Block 6 Stutter parameter " << id << std::endl;
            ++failures;
        }
    }

    if (proc.apvts.getParameter ("filter_mode") == nullptr
        || proc.apvts.getParameter ("filter_slope") == nullptr
        || proc.apvts.getParameter ("filter_resonance") == nullptr)
    {
        std::cout << "  [FAIL] Block 5 filter parameters are missing" << std::endl;
        ++failures;
    }

    if (proc.apvts.getParameter ("space_delay_synced") == nullptr
        || proc.apvts.getParameter ("space_delay_rateunit") == nullptr
        || proc.apvts.getParameter ("space_delay_div") == nullptr)
    {
        std::cout << "  [FAIL] Block 5 delay timing parameters are missing" << std::endl;
        ++failures;
    }

    // Init must be a genuinely clean starting point: every processing module and hidden sync toggle disabled.
    proc.presetManager.loadInitPreset();
    for (auto* id : { "drive", "pan", "volume", "space", "retro", "width", "filter" })
    {
        const juce::String prefix (id);
        for (auto* suffix : { "enabled", "lfo_synced", "motion_synced", "seq_synced" })
        {
            auto* value = proc.apvts.getRawParameterValue (prefix + "_" + suffix);
            if (value == nullptr || value->load() > 0.5f)
            {
                std::cout << "  [FAIL] Init leaves " << prefix << "_" << suffix << " enabled" << std::endl;
                ++failures;
            }
        }
    }

    for (auto* id : { "stutter_enabled", "master_matchgain" })
    {
        auto* value = proc.apvts.getRawParameterValue (id);
        if (value == nullptr || value->load() > 0.5f)
        {
            std::cout << "  [FAIL] Init leaves " << id << " enabled" << std::endl;
            ++failures;
        }
    }

    // Preset identity and dirty state must survive a host session save/restore.
    if (numFactory > 0)
    {
        proc.presetManager.loadFactoryPreset (0);
        const auto expectedName = proc.presetManager.getCurrentName();

        if (proc.presetManager.isCurrentPresetModified())
        {
            std::cout << "  [FAIL] freshly loaded preset is already marked modified" << std::endl;
            ++failures;
        }

        if (auto* input = proc.apvts.getParameter ("master_input"))
            input->setValueNotifyingHost (input->convertTo0to1 (3.0f));

        if (! proc.presetManager.isCurrentPresetModified())
        {
            std::cout << "  [FAIL] parameter edit did not mark the preset modified" << std::endl;
            ++failures;
        }

        juce::MemoryBlock sessionState;
        proc.getStateInformation (sessionState);

        MotionFXAudioProcessor restored;
        restored.setStateInformation (sessionState.getData(), (int) sessionState.getSize());

        if (restored.presetManager.getCurrentName() != expectedName)
        {
            std::cout << "  [FAIL] preset name was not restored with the host session" << std::endl;
            ++failures;
        }

        if (! restored.presetManager.isCurrentPresetModified())
        {
            std::cout << "  [FAIL] modified marker was not restored with the host session" << std::endl;
            ++failures;
        }
    }

    // Build 9 Drive Lab: every algorithm, quality and post stage
    // must remain finite, bounded and audibly active.
    {
        for (int quality = 0;
             quality < 3;
             ++quality)
        {
            for (int postClip = 0;
                 postClip < 4;
                 ++postClip)
            {
                mfx::DriveEffect drive;
                drive.prepare (48000.0, 257);
                drive.setParams (
                    0.25f,
                    1.0f,
                    0.0f,
                    0.35f,
                    quality,
                    postClip);
                drive.setGroovePhaseParams (
                    true,
                    8.0f,
                    684.0f,
                    0.32f,
                    true,
                    8.0f,
                    7500.0f,
                    3.0f,
                    0,
                    true);

                const int expectedFactor =
                    postClip == 3
                        ? 4
                        : quality == 0
                            ? 1
                            : quality == 1
                                ? 2
                                : 4;

                if (drive.getQualityFactor()
                    != expectedFactor)
                {
                    std::cout
                        << "  [FAIL] Drive quality factor mismatch"
                        << std::endl;
                    ++failures;
                }

                if (expectedFactor == 1
                    && drive.getLatencySamples() != 0)
                {
                    std::cout
                        << "  [FAIL] Eco Drive unexpectedly reports latency"
                        << std::endl;
                    ++failures;
                }

                if (expectedFactor > 1
                    && drive.getLatencySamples() <= 0)
                {
                    std::cout
                        << "  [FAIL] Oversampled Drive does not report latency"
                        << std::endl;
                    ++failures;
                }

                for (int mode = 0;
                     mode < 8;
                     ++mode)
                {
                    drive.reset();
                    drive.setMode (
                        (mfx::DriveMode) mode);

                    juce::AudioBuffer<float>
                        driveBuffer (2, 257);
                    double drivePhase = 0.0;
                    bool configurationFailed = false;

                    for (int block = 0;
                         block < 14;
                         ++block)
                    {
                        for (int sample = 0;
                             sample < driveBuffer
                                 .getNumSamples();
                             ++sample)
                        {
                            const float value =
                                0.72f
                                * (float) std::sin (
                                    drivePhase);

                            drivePhase +=
                                juce::MathConstants<double>
                                    ::twoPi
                                * 997.0
                                / 48000.0;

                            if (drivePhase
                                > juce::MathConstants<double>
                                    ::twoPi)
                            {
                                drivePhase -=
                                    juce::MathConstants<double>
                                        ::twoPi;
                            }

                            driveBuffer.setSample (
                                0,
                                sample,
                                value);
                            driveBuffer.setSample (
                                1,
                                sample,
                                value * 0.83f);
                        }

                        drive.processBlock (
                            driveBuffer,
                            0.88f);

                        bool finite = true;
                        float peak = 0.0f;
                        double energy = 0.0;

                        for (int channel = 0;
                             channel < 2;
                             ++channel)
                        {
                            const auto* data =
                                driveBuffer
                                    .getReadPointer (
                                        channel);

                            for (int sample = 0;
                                 sample < driveBuffer
                                     .getNumSamples();
                                 ++sample)
                            {
                                finite =
                                    finite
                                    && std::isfinite (
                                        data[sample]);
                                peak = juce::jmax (
                                    peak,
                                    std::abs (
                                        data[sample]));
                                energy +=
                                    (double) data[sample]
                                    * data[sample];
                            }
                        }

                        const float allowedPeak =
                            postClip == 0
                                ? 4.0f
                                : 1.35f;

                        if (! finite
                            || peak > allowedPeak
                            || energy < 1.0e-8)
                        {
                            std::cout
                                << "  [FAIL] Drive mode "
                                << mode
                                << " quality "
                                << quality
                                << " post "
                                << postClip
                                << " failed integrity checks"
                                << std::endl;
                            ++failures;
                            configurationFailed = true;
                            break;
                        }
                    }

                    if (configurationFailed)
                        continue;
                }
            }
        }

        // Stereo Pinch should produce a measurably different left/right
        // result when the source starts identical.
        mfx::DriveEffect grooveDrive;
        grooveDrive.prepare (48000.0, 512);
        grooveDrive.setMode (
            mfx::DriveMode::GroovePhase);
        grooveDrive.setParams (
            0.0f,
            1.0f,
            0.0f,
            0.0f,
            2,
            0);
        grooveDrive.setGroovePhaseParams (
            false,
            0.0f,
            684.0f,
            0.32f,
            true,
            16.0f,
            7500.0f,
            3.0f,
            1,
            true);

        juce::AudioBuffer<float>
            stereoBuffer (2, 512);

        for (int sample = 0;
             sample < stereoBuffer.getNumSamples();
             ++sample)
        {
            const float value =
                0.55f
                * std::sin (
                    juce::MathConstants<float>::twoPi
                    * 1600.0f
                    * (float) sample
                    / 48000.0f);
            stereoBuffer.setSample (
                0, sample, value);
            stereoBuffer.setSample (
                1, sample, value);
        }

        grooveDrive.processBlock (
            stereoBuffer,
            1.0f);

        double stereoDifference = 0.0;

        for (int sample = 0;
             sample < stereoBuffer.getNumSamples();
             ++sample)
        {
            stereoDifference += std::abs (
                stereoBuffer.getSample (0, sample)
                - stereoBuffer.getSample (1, sample));
        }

        if (stereoDifference < 0.01)
        {
            std::cout
                << "  [FAIL] Groove Phase stereo Pinch produced no stereo phase difference"
                << std::endl;
            ++failures;
        }
    }

    // Block 7 resonance budget.
    for (int stages = 1; stages <= 4; ++stages)
    {
        const float q = mfx::FilterEffect::resonanceQForStages (1.0f, stages);
        const float qPeak = q / std::sqrt (
            1.0f - 1.0f / (4.0f * q * q));
        const float qBoostDb =
            20.0f * (float) stages * std::log10 (qPeak);
        const float peakBoostDb =
            mfx::FilterEffect::peakGainDbPerStage (1.0f, stages)
            * (float) stages;

        if (qBoostDb > 12.001f || peakBoostDb > 12.001f)
        {
            std::cout << "  [FAIL] Filter resonance exceeds 12 dB at "
                      << stages << " stage(s)" << std::endl;
            ++failures;
        }
    }

    // Branching Undo/Redo retains the old redo branch.
    {
        MotionFXAudioProcessor historyProcessor;
        auto* input = historyProcessor.apvts.getParameter ("master_input");
        auto* output = historyProcessor.apvts.getParameter ("master_output");

        if (historyProcessor.stateHistory == nullptr
            || input == nullptr || output == nullptr)
        {
            std::cout << "  [FAIL] State history was not initialised"
                      << std::endl;
            ++failures;
        }
        else
        {
            input->setValueNotifyingHost (input->convertTo0to1 (6.0f));
            historyProcessor.stateHistory->flushPendingNow ("Input edit");

            historyProcessor.stateHistory->undo();
            const float undoneInput = historyProcessor.apvts
                .getRawParameterValue ("master_input")->load();

            historyProcessor.stateHistory->redo();
            const float redoneInput = historyProcessor.apvts
                .getRawParameterValue ("master_input")->load();

            historyProcessor.stateHistory->undo();
            output->setValueNotifyingHost (output->convertTo0to1 (3.0f));
            historyProcessor.stateHistory->flushPendingNow ("Output branch");

            if (std::abs (undoneInput) > 0.01f
                || std::abs (redoneInput - 6.0f) > 0.01f
                || historyProcessor.stateHistory->getBranchCountAt (0) < 2)
            {
                std::cout << "  [FAIL] Branching Undo/Redo history is incorrect"
                          << std::endl;
                ++failures;
            }
        }
    }

    // Hidden preset creator/version metadata and newer-version rejection.
    {
        const auto testDirectory =
            juce::File::getSpecialLocation (juce::File::tempDirectory)
                .getChildFile ("MotionFX_Block7_Preset_Test");

        testDirectory.deleteRecursively();
        testDirectory.createDirectory();
        proc.presetManager.setPresetDirectory (testDirectory);

        const bool saved = proc.presetManager.saveUserPreset (
            "Metadata Test", {}, "Unit Tester");
        const auto presetFile = testDirectory.getChildFile (
            "Metadata Test.mfxpreset");
        auto xml = juce::XmlDocument::parse (presetFile);

        const auto* metadata = xml != nullptr
            ? xml->getChildByName ("FILE_META")
            : nullptr;

        const auto expectedVersion =
            mfx::PresetManager::getCurrentSoftwareVersion();

        if (! saved || metadata == nullptr
            || metadata->getStringAttribute ("creator") != "Unit Tester"
            || metadata->getStringAttribute ("createdWithVersion")
                != expectedVersion)
        {
            std::cout << "  [FAIL] Preset creator/version metadata is missing"
                      << std::endl;
            ++failures;
        }
        else
        {
            auto tooNewText = presetFile.loadFileAsString();
            const auto currentVersionAttribute =
                juce::String ("createdWithVersion=\"")
                + expectedVersion + "\"";

            tooNewText = tooNewText.replace (
                currentVersionAttribute,
                "createdWithVersion=\"99.0.0\"");
            presetFile.replaceWithText (tooNewText);
            proc.presetManager.refreshUserPresetList();

            if (proc.presetManager.loadUserPresetByPath (
                    "Metadata Test.mfxpreset")
                || proc.presetManager.takeLastError().isEmpty())
            {
                std::cout << "  [FAIL] Newer preset version was not rejected"
                          << std::endl;
                ++failures;
            }
        }

        proc.presetManager.setPresetDirectory (
            mfx::PresetManager::getDefaultPresetDirectory());
        testDirectory.deleteRecursively();
    }

    proc.presetManager.loadInitPreset();

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
    for (auto* id : { "drive", "pan", "volume", "space", "retro", "width", "filter" })
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
    if (auto* p = proc.apvts.getParameter ("stutter_pitch_grain_ms"))
        p->setValueNotifyingHost (p->convertTo0to1 (18.0f));
    for (int i = 0; i < 16; ++i)
    {
        const juce::String step (i);
        if (auto* p = proc.apvts.getParameter ("stutter_repeat_step" + step))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) (1 + i % 4)));
        if (auto* p = proc.apvts.getParameter ("stutter_reverse_step" + step))
            p->setValueNotifyingHost (i % 2 == 0 ? 1.0f : 0.0f);
        if (auto* p = proc.apvts.getParameter ("stutter_tape_step" + step))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) (1 + i % 2)));
        if (auto* p = proc.apvts.getParameter ("stutter_pitch_step" + step))
            p->setValueNotifyingHost (i % 3 == 0 ? 1.0f : 0.0f);
        if (auto* p = proc.apvts.getParameter ("stutter_pitch_semitones_step" + step))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) (-24 + (i * 7) % 49)));
        if (auto* p = proc.apvts.getParameter ("stutter_gate_step" + step))
            p->setValueNotifyingHost (i % 5 == 0 ? 1.0f : 0.0f);
    }
    runPass (proc, 44100.0, 128, "Extreme stress");

    std::cout << "=== RESULT: " << failures << " failing checks across " << totalBlocksChecked << " blocks tested ===" << std::endl;
    return failures > 0 ? 1 : 0;
}
