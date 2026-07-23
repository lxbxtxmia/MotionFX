#include "PluginEditor.h"
#include "GUI/LookAndFeel.h"

using namespace mfx;

namespace
{
    MotionFXLookAndFeel& dialogLookAndFeel()
    {
        static MotionFXLookAndFeel lookAndFeel;
        lookAndFeel.refreshColours();
        return lookAndFeel;
    }

    void configureThemedTextEditor (
        juce::TextEditor& editor,
        float fontHeight = 14.0f)
    {
        const auto font = FontBank::font (fontHeight);
        editor.setFont (font);
        editor.applyFontToAllText (font, true);
        editor.setColour (
            juce::TextEditor::backgroundColourId,
            Palette::panel);
        editor.setColour (
            juce::TextEditor::textColourId,
            Palette::text);
        editor.applyColourToAllText (
            Palette::text,
            true);
        editor.setColour (
            juce::TextEditor::highlightColourId,
            Palette::teal.withAlpha (0.30f));
        editor.setColour (
            juce::TextEditor::highlightedTextColourId,
            Palette::text);
        editor.setColour (
            juce::TextEditor::outlineColourId,
            Palette::stroke);
        editor.setColour (
            juce::TextEditor::focusedOutlineColourId,
            Palette::teal);
        editor.setColour (
            juce::CaretComponent::caretColourId,
            Palette::teal);
    }

    void configureThemedAlertWindow (
        juce::AlertWindow& alert)
    {
        alert.setLookAndFeel (&dialogLookAndFeel());
        alert.setColour (
            juce::AlertWindow::backgroundColourId,
            Palette::bg1);
        alert.setColour (
            juce::AlertWindow::textColourId,
            Palette::text);
        alert.setColour (
            juce::AlertWindow::outlineColourId,
            Palette::stroke);
    }

    class AboutDialogContent final : public juce::Component
    {
    public:
        AboutDialogContent (
            const juce::String& aboutText,
            const juce::String& changelogText,
            bool startExpanded)
        {
            setLookAndFeel (&dialogLookAndFeel());

            configureEditor (aboutEditor, aboutText);
            configureEditor (changelogEditor, changelogText);
            addAndMakeVisible (aboutEditor);
            addAndMakeVisible (changelogToggle);
            addAndMakeVisible (changelogEditor);

            changelogToggle.setColour (
                juce::TextButton::textColourOffId,
                Palette::text);
            changelogToggle.setColour (
                juce::TextButton::textColourOnId,
                Palette::text);

            changelogToggle.onClick = [this]
            {
                setExpanded (! expanded);
            };

            setExpanded (startExpanded);
        }

        ~AboutDialogContent() override
        {
            setLookAndFeel (nullptr);
        }

        void paint (juce::Graphics& graphics) override
        {
            graphics.fillAll (Palette::bg0);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (14);
            aboutEditor.setBounds (
                bounds.removeFromTop (250));
            bounds.removeFromTop (9);
            changelogToggle.setBounds (
                bounds.removeFromTop (38));
            bounds.removeFromTop (9);

            if (expanded)
                changelogEditor.setBounds (bounds);
        }

    private:
        static void configureEditor (
            juce::TextEditor& editor,
            const juce::String& text)
        {
            editor.setMultiLine (true);
            editor.setReadOnly (true);
            editor.setScrollbarsShown (true);
            editor.setCaretVisible (false);
            editor.setPopupMenuEnabled (true);
            configureThemedTextEditor (editor, 13.5f);
            editor.setText (text, false);
            editor.moveCaretToTop (false);
        }

        void setExpanded (bool shouldExpand)
        {
            expanded = shouldExpand;
            changelogEditor.setVisible (expanded);
            changelogToggle.setButtonText (
                expanded
                    ? "Hide changelog"
                    : "Show changelog");
            setSize (700, expanded ? 650 : 350);
            resized();

            if (auto* dialog =
                    findParentComponentOfClass<
                        juce::DialogWindow>())
            {
                dialog->setContentComponentSize (
                    getWidth(),
                    getHeight());
            }
        }

        juce::TextEditor aboutEditor;
        juce::TextEditor changelogEditor;
        juce::TextButton changelogToggle;
        bool expanded = false;
    };

    EffectPanelSpec makeSpec (EffectId id)
    {
        EffectPanelSpec s;
        int i = (int) id;
        s.prefix = effectPrefixes[i];
        s.displayName = effectDisplayNames[i];
        s.accent = Palette::effectColour (i);
        s.modeParamId = s.prefix + "_mode";

        switch (id)
        {
            case EffectId::Drive:
                s.secondaryKnobs = {
                    { "drive_tone", "TONE" },
                    { "drive_bias", "BIAS" },
                    { "drive_mix", "MIX" },
                    { "drive_outtrim", "OUT" }
                };
                break;
            case EffectId::Pan:
                s.secondaryKnobs = { { "pan_widthinfluence", "WIDTH INF" } };
                break;
            case EffectId::Volume:
                break;
            case EffectId::Space:
                s.secondaryKnobs = { { "space_size", "SIZE" }, { "space_decay", "DECAY" }, { "space_tone", "TONE" } };
                break;
            case EffectId::Retro:
                s.secondaryKnobs = {
                    { "retro_bits", "BITS" },
                    { "retro_sample_rate", "SAMPLE RATE" },
                    { "retro_lossy_bandwidth", "BANDWIDTH" },
                    { "retro_lossy_detail", "DETAIL" },
                    { "retro_lossy_damage", "DAMAGE" },
                    { "retro_wow", "WOW" },
                    { "retro_flutter", "FLUTTER" },
                    { "retro_dropout", "DROPOUT" },
                    { "retro_age", "AGE" },
                    { "retro_stereo_drift", "STEREO DRIFT" },
                    { "retro_sp_clock", "CLOCK" },
                    { "retro_sp_filter_cutoff", "FILTER" },
                    { "retro_sp_drive", "INPUT DRIVE" },
                    { "retro_tape_drive", "DRIVE" },
                    { "retro_tape_age", "AGE" },
                    { "retro_tape_motion", "MOTION" },
                    { "retro_tape_noise", "NOISE" },
                    { "retro_tape_nr_amount", "NR AMOUNT" },
                    { "retro_tape_denoise", "DENOISE" },
                    { "retro_vinyl_dust", "DUST" },
                    { "retro_vinyl_crackle", "CRACKLE" },
                    { "retro_vinyl_surface", "SURFACE" },
                    { "retro_vinyl_wear", "WEAR" },
                    { "retro_mix", "MIX" }
                };
                break;
            case EffectId::Width:
                s.secondaryKnobs = { { "width_crossover", "X-OVER" } };
                break;
            case EffectId::Filter:
                s.secondaryKnobs = {
                    { "filter_resonance", "RESONANCE" },
                    { "filter_mix", "MIX" }
                };
                break;
        }
        return s;
    }
}

MotionFXAudioProcessorEditor::MotionFXAudioProcessorEditor (MotionFXAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);
    tooltipWindow = std::make_unique<juce::TooltipWindow> (this, 450);

    if (processor.stateHistory != nullptr)
        processor.stateHistory->ensureTimerRunning();

    addAndMakeVisible (content);

    titleLabel.setText ("MOTIONFX", juce::dontSendNotification);
    titleLabel.setFont (FontBank::fixedFont (27.0f, true));
    titleLabel.setColour (juce::Label::textColourId, Palette::teal);
    titleLabel.setTooltip ("About MotionFX");
    titleLabel.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    titleLabel.addMouseListener (this, false);
    content.addAndMakeVisible (titleLabel);

    presetNameButton.setClickingTogglesState (false);
    presetNameButton.setTooltip ("Open preset browser");
    prevPresetBtn.setTooltip ("Previous preset");
    nextPresetBtn.setTooltip ("Next preset");
    undoBtn.setTooltip ("Undo the last MotionFX edit");
    redoBtn.setTooltip ("Redo the preferred history branch");
    savePresetBtn.setTooltip ("Save preset");
    optionsBtn.setTooltip ("Options and change history");
    content.addAndMakeVisible (presetNameButton);
    presetNameButton.onClick = [this] { showPresetMenu(); };

    for (auto* button : {
             &prevPresetBtn, &nextPresetBtn, &undoBtn, &redoBtn,
             &savePresetBtn, &optionsBtn
         })
    {
        content.addAndMakeVisible (button);
    }

    prevPresetBtn.onClick = [this]
    {
        const bool loaded = processor.presetManager.previous();
        showPresetLoadErrorIfAny();
        if (loaded && processor.stateHistory != nullptr)
            processor.stateHistory->notifyExternalChange ("Preset change");
        refreshPresetLabel();
        resized();
    };

    nextPresetBtn.onClick = [this]
    {
        const bool loaded = processor.presetManager.next();
        showPresetLoadErrorIfAny();
        if (loaded && processor.stateHistory != nullptr)
            processor.stateHistory->notifyExternalChange ("Preset change");
        refreshPresetLabel();
        resized();
    };

    undoBtn.onClick = [this]
    {
        if (processor.stateHistory != nullptr)
            processor.stateHistory->undo();
        refreshPresetLabel();
        tabStrip.setOrder (processor.getOrder());
        resized();
    };

    redoBtn.onClick = [this]
    {
        if (processor.stateHistory != nullptr)
            processor.stateHistory->redo();
        refreshPresetLabel();
        tabStrip.setOrder (processor.getOrder());
        resized();
    };

    savePresetBtn.onClick = [this] { savePresetDialog(); };
    optionsBtn.onClick = [this] { showOptionsMenu(); };

    outputMeter = std::make_unique<StereoOutputMeter> (
        processor.chain.outputLeftLevelUi,
        processor.chain.outputRightLevelUi,
        processor.chain.uiSignalEpoch);
    content.addAndMakeVisible (*outputMeter);

    rebuildThemedControls();

    content.addAndMakeVisible (tabStrip);
    tabStrip.setOrder (processor.getOrder());
    tabStrip.onSelect = [this] (int slot) { selectedSlot = slot; tabStrip.setSelectedSlot (slot); resized(); };
    tabStrip.onReorder = [this] (std::array<EffectId, numEffects> newOrder)
    {
        // The selected tab represents an effect identity, not a visual slot.
        // Preserve that identity while the slots move around it.
        const auto oldOrder = processor.getOrder();
        const bool stutterWasSelected = (selectedSlot == numEffects);
        const EffectId selectedEffect = stutterWasSelected
                                          ? EffectId::Drive // unused in this branch
                                          : oldOrder[(size_t) juce::jlimit (0, numEffects - 1, selectedSlot)];

        processor.setOrder (newOrder);

        if (processor.stateHistory != nullptr)
            processor.stateHistory->notifyExternalChange ("Effect order");

        if (! stutterWasSelected)
        {
            for (int slot = 0; slot < numEffects; ++slot)
            {
                if (newOrder[(size_t) slot] == selectedEffect)
                {
                    selectedSlot = slot;
                    break;
                }
            }
        }

        tabStrip.setSelectedSlot (selectedSlot);
        resized();
    };

    constrainer = std::make_unique<juce::ComponentBoundsConstrainer>();
    constrainer->setFixedAspectRatio ((double) baseW / (double) baseH);
    constrainer->setSizeLimits (baseW / 4, baseH / 4, baseW * 3, baseH * 3);
    setConstrainer (constrainer.get());
    setResizable (true, true);

    refreshPresetLabel();
    setScalePercent (100);
    grabKeyboardFocus();
    startTimerHz (8);
}

MotionFXAudioProcessorEditor::~MotionFXAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void MotionFXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::bg0);
}

bool MotionFXAudioProcessorEditor::keyPressed (
    const juce::KeyPress& key)
{
    const auto modifiers = key.getModifiers();
    const bool command = modifiers.isCommandDown()
                      || modifiers.isCtrlDown();

    if (! command)
        return false;

    const int keyCode = key.getKeyCode();

    if (keyCode == 'z' || keyCode == 'Z')
    {
        if (processor.stateHistory != nullptr)
        {
            if (modifiers.isShiftDown())
                processor.stateHistory->redo();
            else
                processor.stateHistory->undo();
        }

        refreshPresetLabel();
        tabStrip.setOrder (processor.getOrder());
        resized();
        return true;
    }

    if (keyCode == 'y' || keyCode == 'Y')
    {
        if (processor.stateHistory != nullptr)
            processor.stateHistory->redo();

        refreshPresetLabel();
        tabStrip.setOrder (processor.getOrder());
        resized();
        return true;
    }

    if (keyCode == 's' || keyCode == 'S')
    {
        savePresetDialog();
        return true;
    }

    return false;
}

void MotionFXAudioProcessorEditor::setScalePercent (int percent)
{
    scalePercent = juce::jlimit (25, 300, percent);
    float scale = scalePercent / 100.0f;
    content.setTransform (juce::AffineTransform::scale (scale));
    setSize ((int) (baseW * scale), (int) (baseH * scale));
}

void MotionFXAudioProcessorEditor::rebuildThemedControls()
{
    inputKnob.reset();
    outputKnob.reset();
    dryWetKnob.reset();
    matchGainToggle.reset();

    for (auto& panel : effectPanels)
        panel.reset();

    stutterPanel.reset();

    titleLabel.setFont (FontBank::fixedFont (27.0f, true));
    titleLabel.setColour (
        juce::Label::textColourId,
        Palette::teal);

    inputKnob = std::make_unique<LabeledKnob> (
        processor.apvts,
        "master_input",
        "INPUT",
        Palette::teal);
    outputKnob = std::make_unique<LabeledKnob> (
        processor.apvts,
        "master_output",
        "OUTPUT",
        Palette::teal);
    dryWetKnob = std::make_unique<LabeledKnob> (
        processor.apvts,
        "master_drywet",
        "DRY/WET",
        Palette::purple);
    matchGainToggle = std::make_unique<LabeledToggle> (
        processor.apvts,
        "master_matchgain",
        "GAIN MATCH");

    inputKnob->setCompactLayout (true);
    outputKnob->setCompactLayout (true);
    dryWetKnob->setCompactLayout (true);

    for (auto* component : {
             (juce::Component*) inputKnob.get(),
             (juce::Component*) outputKnob.get(),
             (juce::Component*) dryWetKnob.get(),
             (juce::Component*) matchGainToggle.get()
         })
    {
        content.addAndMakeVisible (component);
    }

    for (int effect = 0; effect < numEffects; ++effect)
    {
        effectPanels[(size_t) effect] =
            std::make_unique<EffectPanel> (
                processor.apvts,
                processor.chain,
                makeSpec ((EffectId) effect),
                (EffectId) effect);
        content.addAndMakeVisible (
            *effectPanels[(size_t) effect]);
    }

    stutterPanel = std::make_unique<StutterPanel> (
        processor.apvts,
        processor.chain);
    content.addAndMakeVisible (*stutterPanel);

    content.sendLookAndFeelChange();
    tabStrip.repaint();
    resized();
    repaint();
}

void MotionFXAudioProcessorEditor::applyUiPreferences()
{
    lookAndFeel.refreshColours();
    rebuildThemedControls();
}

void MotionFXAudioProcessorEditor::resized()
{
    const float scale =
        getWidth() / (float) baseW;
    scalePercent = juce::roundToInt (
        scale * 100.0f);

    content.setTransform (
        juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, baseW, baseH);

    auto bounds =
        content.getLocalBounds().reduced (14);
    auto header = bounds.removeFromTop (90);

    auto titleArea =
        header.removeFromLeft (142);
    titleLabel.setBounds (
        titleArea.reduced (0, 14));

    // Keep the same visual margin on both sides of the MotionFX wordmark.
    header.removeFromLeft (14);

    auto masterArea =
        header.removeFromRight (390);

    matchGainToggle->setBounds (
        masterArea.removeFromRight (104)
                  .reduced (1, 21));
    masterArea.removeFromRight (8);

    outputMeter->setBounds (
        masterArea.removeFromRight (30)
                  .reduced (0, 8));
    masterArea.removeFromRight (9);

    dryWetKnob->setBounds (
        masterArea.removeFromRight (72)
                  .translated (0, 3));
    masterArea.removeFromRight (5);

    outputKnob->setBounds (
        masterArea.removeFromRight (72)
                  .translated (0, 3));
    masterArea.removeFromRight (5);

    inputKnob->setBounds (
        masterArea.removeFromRight (72)
                  .translated (0, 3));

    auto presetBar = header;

    const auto placeSquareRight =
        [&presetBar] (juce::Component& component,
                      int gapAfter)
        {
            auto cell =
                presetBar.removeFromRight (38);
            component.setBounds (
                cell.withSizeKeepingCentre (36, 36));
            presetBar.removeFromRight (gapAfter);
        };

    placeSquareRight (optionsBtn, 5);
    placeSquareRight (savePresetBtn, 6);
    placeSquareRight (redoBtn, 5);
    placeSquareRight (undoBtn, 8);

    auto nextCell =
        presetBar.removeFromRight (38);
    nextPresetBtn.setBounds (
        nextCell.withSizeKeepingCentre (36, 36));
    presetBar.removeFromRight (5);

    auto previousCell =
        presetBar.removeFromLeft (38);
    prevPresetBtn.setBounds (
        previousCell.withSizeKeepingCentre (36, 36));
    presetBar.removeFromLeft (5);

    presetNameButton.setBounds (
        presetBar.reduced (0, 27));

    bounds.removeFromTop (6);
    tabStrip.setBounds (
        bounds.removeFromTop (44));
    bounds.removeFromTop (9);

    const auto order = processor.getOrder();

    for (int slot = 0;
         slot < numEffects;
         ++slot)
    {
        const bool visible =
            slot == selectedSlot;

        auto* panel = effectPanels[
            (size_t) order[(size_t) slot]].get();

        if (panel == nullptr)
            continue;

        panel->setVisible (visible);

        if (visible)
            panel->setBounds (bounds);
    }

    const bool stutterVisible =
        selectedSlot == numEffects;

    if (stutterPanel != nullptr)
    {
        stutterPanel->setVisible (
            stutterVisible);

        if (stutterVisible)
            stutterPanel->setBounds (bounds);
    }
}

void MotionFXAudioProcessorEditor::refreshPresetLabel()
{
    const auto fullName = processor.presetManager.getCurrentName();
    const bool modified = processor.presetManager.isCurrentPresetModified();

    auto shortened = fullName.length() > 20
        ? fullName.substring (0, 20) + "..."
        : fullName;

    if (modified)
        shortened += "*";

    if (presetNameButton.getButtonText() != shortened)
        presetNameButton.setButtonText (shortened);

    juce::String tooltip = fullName;
    if (modified)
        tooltip += "\nModified";

    tooltip += "\nClick to browse presets";
    presetNameButton.setTooltip (tooltip);
}

void MotionFXAudioProcessorEditor::timerCallback()
{
    tabStrip.setOrder (processor.getOrder());
    refreshPresetLabel();

    if (processor.stateHistory != nullptr)
    {
        undoBtn.setEnabled (processor.stateHistory->canUndo());
        redoBtn.setEnabled (processor.stateHistory->canRedo());
    }
    else
    {
        undoBtn.setEnabled (false);
        redoBtn.setEnabled (false);
    }
}

void MotionFXAudioProcessorEditor::mouseUp (const juce::MouseEvent& event)
{
    if (event.eventComponent == &titleLabel)
        showAboutDialog();
}

void MotionFXAudioProcessorEditor::showAboutDialog (bool openChangelog)
{
    const auto aboutText = juce::String (R"MFXABOUT(MotionFX 0.10.0 - Build 10

Multi-effect modulation VST3.

Direction and development: Paom
Some AI was used during the creation of this plugin, but all generated work was reviewed, reworked and proofed by humans.

Built with JUCE 8, C++20, CMake and the VST3 format.

Interface font
- Atkinson Hyperlegible Next by the Braille Institute project
- Embedded under the SIL Open Font License 1.1

Resources
- JUCE framework
- Steinberg VST3 SDK through JUCE
- GitHub Actions continuous integration

Retro Lab research basis
- Public SP-12/SP-1200 circuit and clock documentation
- Public physical-model research on analog tape machines
- Public spectral-gating descriptions for adaptive noise reduction
- MotionFX uses original DSP code; no third-party GPL source was copied

Click the MOTIONFX title at any time to reopen this window.)MFXABOUT");

    const auto changelogText = juce::String (R"MFXCHANGELOG(0.10.0 - Build 10: Retro Lab
- Rebuilt Retro as six distinct processors: Bitcrush, Lossy, Wear & Tear, SP 12-Bit, Tape and Vinyl Dust.
- Added explicit bit depth, sample-rate hold modes, dither and optional anti-alias filtering.
- Added a real FFT Lossy processor with Eco/Normal/High quality, bandwidth, detail, damage and stereo-link controls.
- Added separate wow, flutter, progressive dropout, age and stereo-drift controls.
- Added an original SP 12-bit real-time model based on the documented 26.04 kHz clock, 12-bit conversion and selectable output-filter families.
- Added Reel-to-Reel and Cassette tape profiles, five tape speeds, machine-specific noise and B-style/C-style companding-inspired noise reduction.
- Added an adaptive high-band tape denoise control based on public spectral-gating concepts.
- Added a dedicated Vinyl Dust processor for surface noise and wear without duplicating Groove Phase distortion.
- Added mode-specific controls, visual feedback, latency reporting and extensive audio integrity tests.

0.9.1 - Build 9.1
- Retuned Groove Phase from the supplied reference IR, restoring its bounded stereo DC punch and cross-channel Pinch geometry.
- Added clickable Gain, Frequency and Bandwidth fields plus a live spectrum behind both Groove Phase curves.
- Corrected Soft post clipping so it remains transparent below its knee and audited zero-Drive transparency across every algorithm.
- Unified labels, numbers and units on Atkinson Hyperlegible Next.
- Themed About/Changelog, Accessibility and preset dialogs with the active MotionFX palette.
- Fixed invisible About/Changelog text and mismatched scrollbars in the Light theme.
- Kept the MotionFX title and Gain Match readable when Larger Interface Text is enabled.
- Double-clicking a knob now restores its real parameter default.
- Grouped straight, triplet and dotted sync divisions together and added matching Stutter beat separators.
- Redrew Undo and Redo as clean return arrows with separated heads.

0.9.0 - Build 9
- Rebuilt the Drive engine with eight algorithms and true JUCE oversampling.
- Added Groove Phase: a crackle-free, band-selective tracing and stereo Pinch distortion model.
- Added draggable Tracing Model and Pinch X-Y controls with Alt/Option bandwidth editing.
- Added Wavefold and Sinoid Fold as separate algorithms.
- Added Drive Bias, Eco/2x/4x oversampling and None/Soft/Hard/True Peak post modes.
- Added latency reporting and dry/wet compensation.
- Redrew all header arrows and increased general/accessibility text sizes.

0.8.1 - Build 8.1
- Refined vector arrows, typography, engineering units and adaptive control geometry.
- Added 0-200% Width and live modulation-range displays.
- Simplified the stereo output meter.

0.8.0 - Build 8
- Rebuilt the header with vector icon controls and compact master knobs.
- Added a post-effect stereo peak meter with green, yellow and red dBFS zones.
- Embedded Atkinson Hyperlegible Next Regular and Bold.
- Added Dark, Light and custom JSON themes.
- Added theme-independent High Contrast, Reduced Motion, Enhanced Controls and Larger Text accessibility options.
- Added keyboard shortcuts for Undo, Redo and Save.
- Expanded GUI tests across themes, contrast modes and interface scales.

0.7.0 - Build 7
- Limited non-Comb filter resonance to a 12 dB total boost.
- Added continuous Stutter playhead and CLEAR refresh.
- Improved Stutter, GAIN MATCH and TEMPO SYNC typography.
- Added branching Undo/Redo history and crash recovery.
- Added compact preset names, author/version metadata and compatibility checks.

0.6.0 - Build 6
- Signal visualisers decay to zero when audio processing stops.
- Dragged tabs use a neutral ghost style.
- Rebuilt Stutter as independent Repeat, Reverse, Tape, Pitch and Gate lanes.
- Added overlapping per-step Stutter processing.
- Added independent per-step realtime granular pitch shifting from -24 to +24 semitones.
- Added left-drag painting, right-drag erasing and click-to-zero Pitch editing.

0.5.0 - Build 5
- Added the Filter category with six modes and selectable slopes.
- Added live input/output signal traces behind modulation displays.
- Changed knob double-click behaviour and fixed two-decimal displays.
- Added contextual delay/reverb labels and delay timing in Hz, seconds or tempo divisions.
- Simplified Stutter controls and dragged-tab visuals.

0.4.0 - Build 4
- Reworked the interface hierarchy and control sizing.
- Unified effect and modulation knob sizes.
- Added two-decimal numeric displays and direct value entry.
- Added Hz, seconds and tempo-synced timing for LFO, Motion and Sequencer modulators.
- Added dotted and additional triplet timing divisions while preserving old preset indices.
- Enlarged mode selectors and reorganised related modulation controls.
- Corrected Stutter repeat lengths so 1/4, 1/8, 1/16 and 1/32 are absolute musical values.
- Added Stutter loop-boundary crossfades, Clear and alternating 1/8 helpers.
- Consolidated About and Changelog into one expandable window.
- Registered the audio and GUI executables with CTest.

0.3.0 - Build 3
- Preset identity and modified-state persistence.
- Clean Init state.
- Header, preset and modulation-source readability fixes.
- Stable drag-and-drop tab identity.
- Direct numeric value entry and compact decimals.
- About, resources and changelog windows.

0.2.0 - Build 2
- Preset browser and recursive user folders.
- Portable CMake and automated Windows/Linux builds.

0.1.0 - Build 1
- DSP pause on stopped host transport.
- Selected-effect identity preserved during reorder.
- Gain Match naming update.)MFXCHANGELOG");

    auto* contentComponent =
        new AboutDialogContent (
            aboutText,
            changelogText,
            openChangelog);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (contentComponent);
    options.dialogTitle = "About MotionFX";
    options.dialogBackgroundColour = Palette::bg0;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = true;
    options.componentToCentreAround = this;

    if (auto* dialog = options.launchAsync())
    {
        dialog->setLookAndFeel (&dialogLookAndFeel());
        dialog->setColour (
            juce::ResizableWindow::backgroundColourId,
            Palette::bg0);
        dialog->setColour (
            juce::DocumentWindow::textColourId,
            Palette::text);
        dialog->sendLookAndFeelChange();
    }
}

void MotionFXAudioProcessorEditor::showChangelogDialog()
{
    showAboutDialog (true);
}

void MotionFXAudioProcessorEditor::showPresetMenu()
{
    processor.presetManager.refreshUserPresetList();
    juce::PopupMenu menu;
    int itemId = 1;

    menu.addItem (itemId++, "Init", true, processor.presetManager.getCurrentName() == "Init");

    juce::PopupMenu factoryMenu;
    for (int i = 0; i < processor.presetManager.getNumFactoryPresets(); ++i)
        factoryMenu.addItem (itemId++, processor.presetManager.getFactoryPresetName (i));
    menu.addSubMenu ("Factory Presets", factoryMenu);

    const auto& users = processor.presetManager.getUserPresets();
    if (! users.empty())
    {
        // JUCE PopupMenu submenus are value objects, so build a simple tree recursively
        // from the real directory structure. The result IDs still map to combined indices.
        std::function<juce::PopupMenu (const juce::String&)> buildFolder;
        buildFolder = [&] (const juce::String& prefix)
        {
            juce::PopupMenu result;
            juce::StringArray childFolders;
            for (int i = 0; i < (int) users.size(); ++i)
            {
                auto path = users[(size_t) i].relativePath.replaceCharacter ('\\', '/');
                auto dir = path.upToLastOccurrenceOf ("/", false, false);
                auto leaf = path.fromLastOccurrenceOf ("/", false, false);
                if (prefix.isEmpty() ? dir.isEmpty() : dir == prefix)
                    result.addItem (1 + processor.presetManager.getNumFactoryPresets() + i + 1, leaf.upToLastOccurrenceOf (".", false, false));
                else if (prefix.isEmpty() ? dir.isNotEmpty() : dir.startsWith (prefix + "/"))
                {
                    auto remainder = prefix.isEmpty() ? dir : dir.substring (prefix.length() + 1);
                    auto child = remainder.upToFirstOccurrenceOf ("/", false, false);
                    if (child.isEmpty()) child = remainder;
                    childFolders.addIfNotAlreadyThere (child);
                }
            }
            childFolders.sort (true);
            for (auto& child : childFolders)
            {
                auto childPrefix = prefix.isEmpty() ? child : prefix + "/" + child;
                result.addSubMenu (child, buildFolder (childPrefix));
            }
            return result;
        };
        menu.addSubMenu ("User Presets", buildFolder ({}));
    }

    menu.addSeparator();
    menu.addItem (9001, "Save Preset...");
    menu.addItem (9002, "Create Preset Folder...");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetNameButton), [this] (int result)
    {
        if (result > 0 && result < 9000)
        {
            const bool loaded = processor.presetManager.loadByCombinedIndex (
                result - 1);
            showPresetLoadErrorIfAny();

            if (loaded && processor.stateHistory != nullptr)
                processor.stateHistory->notifyExternalChange ("Preset change");

            refreshPresetLabel();
            resized();
        }
        else if (result == 9001) savePresetDialog();
        else if (result == 9002) createPresetFolderDialog();
    });
}

void MotionFXAudioProcessorEditor::savePresetDialog()
{
    auto alert = std::make_shared<juce::AlertWindow> (
        "Save Preset",
        "Enter a preset name, optional folder and author metadata:",
        juce::MessageBoxIconType::NoIcon);

    configureThemedAlertWindow (*alert);

    alert->addTextEditor (
        "name",
        processor.presetManager.getCurrentName() == "Init"
            ? "New Preset"
            : processor.presetManager.getCurrentName());
    alert->addTextEditor ("folder", "User Made");
    alert->addTextEditor (
        "author",
        mfx::PresetManager::getDefaultAuthor());

    for (const auto& editorId : {
             juce::String ("name"),
             juce::String ("folder"),
             juce::String ("author")
         })
    {
        if (auto* editor = alert->getTextEditor (editorId))
            configureThemedTextEditor (*editor, 14.0f);
    }

    alert->addButton (
        "Save",
        1,
        juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton (
        "Cancel",
        0,
        juce::KeyPress (juce::KeyPress::escapeKey));

    alert->enterModalState (
        true,
        juce::ModalCallbackFunction::create (
            [this, alert] (int result) mutable
    {
        if (result == 1)
        {
            const auto name =
                alert->getTextEditorContents ("name").trim();
            const auto folder =
                alert->getTextEditorContents ("folder").trim();
            const auto author =
                alert->getTextEditorContents ("author").trim();

            const bool saved =
                processor.presetManager.saveUserPreset (
                    name,
                    folder,
                    author);

            showPresetLoadErrorIfAny();

            if (saved)
            {
                if (processor.stateHistory != nullptr)
                {
                    processor.stateHistory
                        ->notifyExternalChange (
                            "Preset saved");
                }

                refreshPresetLabel();
            }
        }

        alert->setLookAndFeel (nullptr);
        alert.reset();
    }));
}

void MotionFXAudioProcessorEditor::createPresetFolderDialog()
{
    auto alert = std::make_shared<juce::AlertWindow> (
        "Create Preset Folder",
        "Folder path relative to the preset library:",
        juce::MessageBoxIconType::NoIcon);

    configureThemedAlertWindow (*alert);
    alert->addTextEditor ("folder", "User Made");

    if (auto* editor = alert->getTextEditor ("folder"))
        configureThemedTextEditor (*editor, 14.0f);

    alert->addButton (
        "Create",
        1,
        juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton (
        "Cancel",
        0,
        juce::KeyPress (juce::KeyPress::escapeKey));

    alert->enterModalState (
        true,
        juce::ModalCallbackFunction::create (
            [this, alert] (int result) mutable
    {
        if (result == 1)
        {
            processor.presetManager.createPresetFolder (
                alert->getTextEditorContents (
                    "folder").trim());
        }

        alert->setLookAndFeel (nullptr);
        alert.reset();
    }));
}

void MotionFXAudioProcessorEditor::showPresetLoadErrorIfAny()
{
    const auto error =
        processor.presetManager.takeLastError();

    if (error.isEmpty())
        return;

    auto alert = std::make_shared<juce::AlertWindow> (
        "MotionFX Preset",
        error,
        juce::MessageBoxIconType::WarningIcon);

    configureThemedAlertWindow (*alert);
    alert->addButton (
        "OK",
        1,
        juce::KeyPress (juce::KeyPress::returnKey));

    alert->enterModalState (
        true,
        juce::ModalCallbackFunction::create (
            [alert] (int) mutable
    {
        alert->setLookAndFeel (nullptr);
        alert.reset();
    }));
}

void MotionFXAudioProcessorEditor::showAccessibilityDialog()
{
    juce::Component::SafePointer<
        MotionFXAudioProcessorEditor> safeThis (this);

    auto* contentComponent =
        new AccessibilityDialogContent (
            [safeThis]
            {
                if (safeThis != nullptr)
                    safeThis->applyUiPreferences();
            });

    contentComponent->setLookAndFeel (
        &dialogLookAndFeel());

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (contentComponent);
    options.dialogTitle = "MotionFX Accessibility";
    options.dialogBackgroundColour = Palette::bg0;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;
    options.componentToCentreAround = this;

    if (auto* dialog = options.launchAsync())
    {
        dialog->setLookAndFeel (&dialogLookAndFeel());
        dialog->setColour (
            juce::ResizableWindow::backgroundColourId,
            Palette::bg0);
        dialog->setColour (
            juce::DocumentWindow::textColourId,
            Palette::text);
        dialog->sendLookAndFeelChange();
    }
}

void MotionFXAudioProcessorEditor::showOptionsMenu()
{
    juce::PopupMenu menu;
    juce::PopupMenu scaleMenu;

    for (int percent : { 25, 50, 75, 100, 150, 200, 300 })
    {
        scaleMenu.addItem (
            1000 + percent,
            juce::String (percent) + "%",
            true,
            scalePercent == percent);
    }

    menu.addSubMenu ("Interface Scale", scaleMenu);

    const auto themes =
        UiPreferences::instance().getAvailableThemes();
    juce::PopupMenu themeMenu;

    for (int index = 0;
         index < (int) themes.size();
         ++index)
    {
        themeMenu.addItem (
            40000 + index,
            themes[(size_t) index].name,
            true,
            themes[(size_t) index].id
                == UiPreferences::instance().getThemeId());
    }

    themeMenu.addSeparator();
    themeMenu.addItem (14, "Open Theme Folder");
    menu.addSubMenu ("Theme", themeMenu);
    menu.addItem (13, "Accessibility...");
    menu.addSeparator();

    if (processor.stateHistory != nullptr)
    {
        menu.addItem (
            10,
            "Undo",
            processor.stateHistory->canUndo());
        menu.addItem (
            11,
            "Redo",
            processor.stateHistory->canRedo());

        juce::PopupMenu historyMenu;
        const auto items =
            processor.stateHistory->getHistoryItems();

        for (const auto& item : items)
        {
            const auto indentation =
                juce::String::repeatedString (
                    "  ",
                    juce::jmin (4, item.depth));

            historyMenu.addItem (
                20000 + item.nodeIndex,
                indentation + item.text,
                true,
                item.current);
        }

        menu.addSubMenu (
            "Change History",
            historyMenu,
            ! items.empty());

        if (processor.stateHistory->hasCrashRecovery())
        {
            menu.addItem (
                12,
                "Restore Last Crash Recovery...");
        }

        menu.addSeparator();
    }

    menu.addItem (3, "Open Preset Browser...");
    menu.addItem (4, "Save Preset...");
    menu.addItem (5, "Create Preset Folder...");
    menu.addSeparator();
    menu.addItem (1, "Choose Preset Folder...");
    menu.addItem (2, "Reset Preset Folder to Default");
    menu.addItem (6, "Open Preset Folder");
    menu.addSeparator();
    menu.addItem (7, "About / Changelog...");

    menu.showMenuAsync (
        juce::PopupMenu::Options()
            .withTargetComponent (&optionsBtn),
        [this, themes] (int result)
    {
        if (result >= 40000)
        {
            const int themeIndex = result - 40000;

            if (themeIndex >= 0
                && themeIndex < (int) themes.size())
            {
                UiPreferences::instance().setThemeId (
                    themes[(size_t) themeIndex].id);
                applyUiPreferences();
            }
        }
        else if (result >= 20000)
        {
            if (processor.stateHistory != nullptr)
                processor.stateHistory->jumpToNode (
                    result - 20000);

            refreshPresetLabel();
            tabStrip.setOrder (processor.getOrder());
            resized();
        }
        else if (result >= 1000)
        {
            setScalePercent (result - 1000);
        }
        else if (result == 13)
        {
            showAccessibilityDialog();
        }
        else if (result == 14)
        {
            UiPreferences::instance()
                .getThemeFolder()
                .startAsProcess();
        }
        else if (result == 10)
        {
            if (processor.stateHistory != nullptr)
                processor.stateHistory->undo();

            refreshPresetLabel();
            tabStrip.setOrder (processor.getOrder());
            resized();
        }
        else if (result == 11)
        {
            if (processor.stateHistory != nullptr)
                processor.stateHistory->redo();

            refreshPresetLabel();
            tabStrip.setOrder (processor.getOrder());
            resized();
        }
        else if (result == 12)
        {
            if (processor.stateHistory != nullptr
                && processor.stateHistory
                       ->restoreCrashRecovery())
            {
                refreshPresetLabel();
                tabStrip.setOrder (processor.getOrder());
                resized();
            }
        }
        else if (result == 3)
        {
            showPresetMenu();
        }
        else if (result == 4)
        {
            savePresetDialog();
        }
        else if (result == 5)
        {
            createPresetFolderDialog();
        }
        else if (result == 1)
        {
            auto chooser =
                std::make_shared<juce::FileChooser> (
                    "Choose a folder for MotionFX presets",
                    processor.presetManager
                        .getPresetDirectory());

            chooser->launchAsync (
                juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent
                          ::canSelectDirectories,
                [this, chooser] (
                    const juce::FileChooser& fileChooser)
            {
                const auto directory =
                    fileChooser.getResult();

                if (directory != juce::File())
                {
                    processor.presetManager
                        .setPresetDirectory (directory);
                }
            });
        }
        else if (result == 2)
        {
            processor.presetManager.setPresetDirectory (
                PresetManager::getDefaultPresetDirectory());
        }
        else if (result == 6)
        {
            processor.presetManager
                .getPresetDirectory()
                .startAsProcess();
        }
        else if (result == 7)
        {
            showAboutDialog();
        }
    });
}
