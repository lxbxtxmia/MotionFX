#!/usr/bin/env python3
"""Apply MotionFX Block 3 to the current repository checkout.

Usage from repository root:
    python apply_block3.py

The script validates every expected source fragment before writing anything.
"""
from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd().resolve()

changes: dict[str, list[tuple[str, str]]] = {
    ".github/workflows/build.yml": [
        ("actions/checkout@v4", "actions/checkout@v5"),
        ("actions/upload-artifact@v4", "actions/upload-artifact@v6"),
    ],
    "MotionFX_source/CMakeLists.txt": [
        ("project(MotionFX VERSION 0.2.0)", "project(MotionFX VERSION 0.3.0)"),
    ],
    "MotionFX_source/Source/PluginEditor.h": [
        (
            '    juce::TextButton prevPresetBtn { "<" }, nextPresetBtn { ">" }, presetMenuBtn { juce::CharPointer_UTF8 ("\\xe2\\x98\\xb0") };',
            '    juce::TextButton prevPresetBtn { "<" }, nextPresetBtn { ">" };',
        ),
        (
            """    void paint (juce::Graphics&) override;
    void resized() override;""",
            """    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;""",
        ),
        (
            """    void showPresetMenu();
    void showOptionsMenu();
    void refreshPresetLabel();""",
            """    void showPresetMenu();
    void showOptionsMenu();
    void showAboutDialog();
    void showChangelogDialog();
    void showScrollableTextDialog (const juce::String& title, const juce::String& text);
    void refreshPresetLabel();""",
        ),
    ],
    "MotionFX_source/Source/PluginEditor.cpp": [
        (
            "    for (auto* btn : { &prevPresetBtn, &nextPresetBtn, &presetMenuBtn, &savePresetBtn, &optionsBtn })",
            "    for (auto* btn : { &prevPresetBtn, &nextPresetBtn, &savePresetBtn, &optionsBtn })",
        ),
        ('    presetMenuBtn.onClick = [this] { showPresetMenu(); };\n', ''),
        (
            '''    optionsBtn.setBounds (presetBar.removeFromRight (36).reduced (2, 10));
    presetBar.removeFromRight (4);
    savePresetBtn.setBounds (presetBar.removeFromRight (56).reduced (0, 12));
    presetBar.removeFromRight (4);
    nextPresetBtn.setBounds (presetBar.removeFromRight (28).reduced (0, 10));
    presetMenuBtn.setBounds (presetBar.removeFromRight (28).reduced (0, 10));
    prevPresetBtn.setBounds (presetBar.removeFromLeft (28).reduced (0, 10));
    presetNameButton.setBounds (presetBar);''',
            '''    optionsBtn.setBounds (presetBar.removeFromRight (36).reduced (2, 12));
    presetBar.removeFromRight (4);
    savePresetBtn.setBounds (presetBar.removeFromRight (56).reduced (0, 12));
    presetBar.removeFromRight (4);
    nextPresetBtn.setBounds (presetBar.removeFromRight (28).reduced (0, 12));
    prevPresetBtn.setBounds (presetBar.removeFromLeft (28).reduced (0, 12));
    presetNameButton.setBounds (presetBar.reduced (0, 12));''',
        ),
        (
            '''void MotionFXAudioProcessorEditor::refreshPresetLabel()
{
    presetNameButton.setButtonText (processor.presetManager.getCurrentName());
}''',
            '''void MotionFXAudioProcessorEditor::refreshPresetLabel()
{
    const auto displayName = processor.presetManager.getDisplayName();
    if (presetNameButton.getButtonText() != displayName)
        presetNameButton.setButtonText (displayName);

    presetNameButton.setTooltip (processor.presetManager.isCurrentPresetModified()
                                     ? "Preset modified — click to browse presets"
                                     : "Open preset browser");
}''',
        ),
        (
            '''void MotionFXAudioProcessorEditor::timerCallback()
{
    tabStrip.setOrder (processor.getOrder());
}''',
            '''void MotionFXAudioProcessorEditor::timerCallback()
{
    tabStrip.setOrder (processor.getOrder());
    refreshPresetLabel();
}''',
        ),
        (
            '''namespace
{
    EffectPanelSpec makeSpec (EffectId id)''',
            '''namespace
{
    class ScrollableTextDialogContent final : public juce::Component
    {
    public:
        explicit ScrollableTextDialogContent (const juce::String& text)
        {
            editor.setMultiLine (true);
            editor.setReadOnly (true);
            editor.setScrollbarsShown (true);
            editor.setText (text, false);
            editor.setColour (juce::TextEditor::backgroundColourId, Palette::bg1);
            editor.setColour (juce::TextEditor::textColourId, Palette::text);
            editor.setColour (juce::TextEditor::outlineColourId, Palette::stroke);
            addAndMakeVisible (editor);
            setSize (620, 440);
        }

        void resized() override
        {
            editor.setBounds (getLocalBounds().reduced (12));
        }

    private:
        juce::TextEditor editor;
    };

    EffectPanelSpec makeSpec (EffectId id)''',
        ),
        (
            '''    titleLabel.setColour (juce::Label::textColourId, Palette::teal);
    content.addAndMakeVisible (titleLabel);''',
            '''    titleLabel.setColour (juce::Label::textColourId, Palette::teal);
    titleLabel.setTooltip ("About MotionFX");
    titleLabel.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    titleLabel.addMouseListener (this, false);
    content.addAndMakeVisible (titleLabel);''',
        ),
        (
            '''    presetNameButton.setClickingTogglesState (false);
    presetNameButton.setTooltip ("Open preset browser");''',
            '''    presetNameButton.setClickingTogglesState (false);
    presetNameButton.setTooltip ("Open preset browser");
    prevPresetBtn.setTooltip ("Previous preset");
    nextPresetBtn.setTooltip ("Next preset");
    savePresetBtn.setTooltip ("Save preset");
    optionsBtn.setTooltip ("Options");''',
        ),
        (
            '''void MotionFXAudioProcessorEditor::showPresetMenu()
{''',
            '''void MotionFXAudioProcessorEditor::mouseUp (const juce::MouseEvent& event)
{
    if (event.eventComponent == &titleLabel)
        showAboutDialog();
}

void MotionFXAudioProcessorEditor::showScrollableTextDialog (const juce::String& title, const juce::String& text)
{
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (new ScrollableTextDialogContent (text));
    options.dialogTitle = title;
    options.dialogBackgroundColour = Palette::bg0;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}

void MotionFXAudioProcessorEditor::showAboutDialog()
{
    showScrollableTextDialog ("About MotionFX", R"MFXABOUT(MotionFX 0.3.0 — Build 3

Multi-effect modulation VST3.

Direction and development: lxbxtxmia
Development assistance: Claude and ChatGPT

Built with JUCE 8, C++20, CMake and the VST3 format.

Resources
- JUCE framework
- Steinberg VST3 SDK through JUCE
- GitHub Actions continuous integration

Click the MOTIONFX title at any time to reopen this window.)MFXABOUT");
}

void MotionFXAudioProcessorEditor::showChangelogDialog()
{
    showScrollableTextDialog ("MotionFX Changelog", R"MFXCHANGELOG(0.3.0 - Block 3
- Preset identity and modified-state persistence
- Clean Init state
- Header and modulation-source readability fixes
- Stable drag-and-drop tab identity
- Direct numeric value entry and compact decimals
- About, resources and changelog windows

Block 2
- Preset browser and recursive user folders
- Portable CMake and automated builds

Block 1
- DSP pause on stopped host transport
- Selected-effect identity preserved during reorder
- Gain Match naming update)MFXCHANGELOG");
}

void MotionFXAudioProcessorEditor::showPresetMenu()
{''',
        ),
        (
            '''    menu.addItem (1, "Choose Preset Folder...");
    menu.addItem (2, "Reset Preset Folder to Default");''',
            '''    menu.addItem (1, "Choose Preset Folder...");
    menu.addItem (2, "Reset Preset Folder to Default");
    menu.addItem (6, "Open Preset Folder");
    menu.addSeparator();
    menu.addItem (7, "About MotionFX...");
    menu.addItem (8, "Changelog...");''',
        ),
        (
            '''        else if (result == 2)
        {
            processor.presetManager.setPresetDirectory (mfx::PresetManager::getDefaultPresetDirectory());
        }''',
            '''        else if (result == 2)
        {
            processor.presetManager.setPresetDirectory (mfx::PresetManager::getDefaultPresetDirectory());
        }
        else if (result == 6)
        {
            processor.presetManager.getPresetDirectory().startAsProcess();
        }
        else if (result == 7) showAboutDialog();
        else if (result == 8) showChangelogDialog();''',
        ),
    ],
    "MotionFX_source/Source/GUI/TabStrip.h": [
        (
            '''                juce::Colour accent = Palette::effectColour ((int) order[(size_t) dragStartSlot]);
                drawTab (g, cell, accent, effectDisplayNames[(int) order[(size_t) dragStartSlot]], true, true);''',
            '''                juce::Colour accent = Palette::effectColour ((int) draggedEffect);
                drawTab (g, cell, accent, effectDisplayNames[(int) draggedEffect], true, true);''',
        ),
        (
            '''            dragging = false; // becomes true only once the mouse actually moves past a threshold
            movedEnough = false;''',
            '''            dragging = false; // becomes true only once the mouse actually moves past a threshold
            movedEnough = false;
            if (slot < numEffects)
                draggedEffect = order[(size_t) slot];''',
        ),
        (
            '''        bool dragging = false, movedEnough = false;
    };''',
            '''        bool dragging = false, movedEnough = false;
        EffectId draggedEffect = EffectId::Drive;
    };''',
        ),
    ],
    "MotionFX_source/Source/GUI/Widgets.h": [
        (
            '''namespace mfx
{
    //==============================================================================
    class LabeledKnob''',
            '''namespace mfx
{
    class EditableSlider : public juce::Slider
    {
    public:
        void mouseDoubleClick (const juce::MouseEvent& event) override
        {
            if (isTextBoxEditable())
            {
                showTextBox();
                return;
            }

            juce::Slider::mouseDoubleClick (event);
        }
    };

    //==============================================================================
    class LabeledKnob''',
        ),
        (
            '''            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
        }''',
            '''            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);

            if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (paramId)))
            {
                const auto interval = parameter->getNormalisableRange().interval;
                slider.setNumDecimalPlacesToDisplay (interval >= 1.0f ? 0 : 3);
            }
        }''',
        ),
        ('        juce::Slider slider;', '        EditableSlider slider;'),
        (
            '''            if (label.getText().isNotEmpty())
                label.setBounds (b.removeFromTop (14));
            combo.setBounds (b);''',
            '''            if (label.getText().isNotEmpty())
                label.setBounds (b.removeFromTop (16));
            combo.setBounds (b.reduced (0, 1));''',
        ),
    ],
    "MotionFX_source/Source/GUI/LookAndFeel.h": [
        (
            '''            g.setColour (Palette::textDim);
            g.fillPath (arrow);
        }

        juce::Font getLabelFont''',
            '''            g.setColour (Palette::textDim);
            g.fillPath (arrow);
        }

        juce::Font getComboBoxFont (juce::ComboBox& box) override
        {
            const auto size = juce::jlimit (12.0f, 16.0f, box.getHeight() * 0.45f);
            return juce::Font (juce::FontOptions (size));
        }

        juce::Font getPopupMenuFont() override
        {
            return juce::Font (juce::FontOptions (14.0f));
        }

        juce::Font getLabelFont''',
        ),
    ],
    "MotionFX_source/Source/GUI/EffectPanel.h": [
        (
            '''            auto top = left.removeFromTop (62);
            modDepthKnob->setBounds (top.removeFromRight (60));
            top.removeFromRight (6);
            modSourceCombo->setBounds (top.removeFromTop (26));''',
            '''            auto top = left.removeFromTop (76);
            modDepthKnob->setBounds (top.removeFromRight (70));
            top.removeFromRight (8);
            modSourceCombo->setBounds (top.removeFromTop (48));''',
        ),
    ],
    "MotionFX_source/Source/Parameters.h": [
        (
            '            juce::ParameterID (pidFor (prefix, "enabled"), 1), prefix + " Enabled", true));',
            '            juce::ParameterID (pidFor (prefix, "enabled"), 1), prefix + " Enabled", false));',
        ),
        (
            '            juce::ParameterID (pidFor (prefix, "lfo_synced"), 1), prefix + " LFO Synced", true));',
            '            juce::ParameterID (pidFor (prefix, "lfo_synced"), 1), prefix + " LFO Synced", false));',
        ),
        (
            '            juce::ParameterID (pidFor (prefix, "motion_synced"), 1), prefix + " Motion Synced", true));',
            '            juce::ParameterID (pidFor (prefix, "motion_synced"), 1), prefix + " Motion Synced", false));',
        ),
        (
            '            juce::ParameterID (pidFor (prefix, "seq_synced"), 1), prefix + " Seq Synced", true));',
            '            juce::ParameterID (pidFor (prefix, "seq_synced"), 1), prefix + " Seq Synced", false));',
        ),
    ],
    "MotionFX_source/Source/PresetManager.h": [
        (
            '''            if (defaultStateXml.isEmpty()) defaultStateXml = buildStateXmlString();
            refreshUserPresetList();''',
            '''            if (defaultStateXml.isEmpty()) defaultStateXml = buildStateXmlString (false);
            refreshUserPresetList();
            markCurrentStateClean();''',
        ),
        (
            '''        juce::String getCurrentName() const noexcept { return currentName; }
        void next();''',
            '''        juce::String getCurrentName() const noexcept { return currentName; }
        juce::String getDisplayName() const;
        bool isCurrentPresetModified() const;
        void next();''',
        ),
        (
            '''        juce::String getFullStateXml() const { return buildStateXmlString(); }
        void restoreFullStateXml (const juce::String& xml) { applyStateXmlString (xml); }

    private:
        juce::String buildStateXmlString() const;
        void applyStateXmlString (const juce::String& xml);''',
            '''        juce::String getFullStateXml() const { return buildStateXmlString (true); }
        void restoreFullStateXml (const juce::String& xml) { applyStateXmlString (xml, true); }

    private:
        juce::String buildStateXmlString (bool includePresetMetadata) const;
        void applyStateXmlString (const juce::String& xml, bool restorePresetMetadata = false);
        juce::int64 computeStateHash() const;
        void markCurrentStateClean();''',
        ),
        (
            '''        juce::String currentName = "Init";
        juce::String defaultStateXml;
    };''',
            '''        juce::String currentName = "Init";
        juce::String defaultStateXml;
        juce::int64 cleanStateHash = 0;
        bool hasCleanStateHash = false;
    };''',
        ),
    ],
    "MotionFX_source/Source/PresetManager.cpp": [
        (
            '''        if (defaultStateXml.isEmpty() && apvts != nullptr) defaultStateXml = buildStateXmlString();
        applyStateXmlString (defaultStateXml);''',
            '''        if (defaultStateXml.isEmpty() && apvts != nullptr) defaultStateXml = buildStateXmlString (false);
        applyStateXmlString (defaultStateXml);''',
        ),
        (
            '''        currentName = "Init";
        currentIndex = 0;
    }''',
            '''        currentName = "Init";
        currentIndex = 0;
        markCurrentStateClean();
    }''',
        ),
        (
            '''        currentName = fps[(size_t) index].name;
        currentIndex = 1 + index;
    }''',
            '''        currentName = fps[(size_t) index].name;
        currentIndex = 1 + index;
        markCurrentStateClean();
    }''',
        ),
        (
            '        if (defaultStateXml.isEmpty() && apvts != nullptr) defaultStateXml = buildStateXmlString();',
            '        if (defaultStateXml.isEmpty() && apvts != nullptr) defaultStateXml = buildStateXmlString (false);',
        ),
        (
            '    juce::String PresetManager::buildStateXmlString() const',
            '    juce::String PresetManager::buildStateXmlString (bool includePresetMetadata) const',
        ),
        (
            '''        }
        return xml->toString();
    }

    void PresetManager::applyStateXmlString (const juce::String& xmlString)
    {
        if (apvts == nullptr || xmlString.isEmpty()) return;
        auto xml = juce::XmlDocument::parse (xmlString);
        if (xml == nullptr) return;
        if (auto* orderXml = xml->getChildByName ("ORDER"))''',
            '''        }

        if (includePresetMetadata)
        {
            auto* metadata = xml->createNewChildElement ("PRESET_META");
            metadata->setAttribute ("name", currentName);
            metadata->setAttribute ("index", currentIndex);
            metadata->setAttribute ("hasCleanHash", hasCleanStateHash ? 1 : 0);
            metadata->setAttribute ("cleanHash", juce::String (cleanStateHash));
        }

        return xml->toString();
    }

    void PresetManager::applyStateXmlString (const juce::String& xmlString, bool restorePresetMetadata)
    {
        if (apvts == nullptr || xmlString.isEmpty()) return;
        auto xml = juce::XmlDocument::parse (xmlString);
        if (xml == nullptr) return;

        bool restoredMetadata = false;
        if (auto* metadata = xml->getChildByName ("PRESET_META"))
        {
            if (restorePresetMetadata)
            {
                currentName = metadata->getStringAttribute ("name", "Restored Session");
                currentIndex = metadata->getIntAttribute ("index", 0);
                hasCleanStateHash = metadata->getBoolAttribute ("hasCleanHash", false);
                cleanStateHash = metadata->getStringAttribute ("cleanHash").getLargeIntValue();
                restoredMetadata = true;
            }

            xml->removeChildElement (metadata, true);
        }

        if (auto* orderXml = xml->getChildByName ("ORDER"))''',
        ),
        (
            '''        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.isValid()) apvts->replaceState (tree);
    }

    bool PresetManager::createPresetFolder''',
            '''        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.isValid())
        {
            apvts->replaceState (tree);

            if (restorePresetMetadata && (! restoredMetadata || ! hasCleanStateHash))
            {
                if (! restoredMetadata)
                {
                    currentName = "Restored Session";
                    currentIndex = 0;
                }
                markCurrentStateClean();
            }
        }
    }

    juce::int64 PresetManager::computeStateHash() const
    {
        return buildStateXmlString (false).hashCode64();
    }

    void PresetManager::markCurrentStateClean()
    {
        cleanStateHash = computeStateHash();
        hasCleanStateHash = true;
    }

    bool PresetManager::isCurrentPresetModified() const
    {
        return hasCleanStateHash && computeStateHash() != cleanStateHash;
    }

    juce::String PresetManager::getDisplayName() const
    {
        return currentName + (isCurrentPresetModified() ? "*" : "");
    }

    bool PresetManager::createPresetFolder''',
        ),
        ('file.replaceWithText (buildStateXmlString())', 'file.replaceWithText (buildStateXmlString (false))'),
        (
            '''                if (userPresets[(size_t) i].relativePath == relative)
                    currentIndex = 1 + getNumFactoryPresets() + i;
        }
        return ok;''',
            '''                if (userPresets[(size_t) i].relativePath == relative)
                    currentIndex = 1 + getNumFactoryPresets() + i;
            markCurrentStateClean();
        }
        return ok;''',
        ),
        (
            '''            if (userPresets[(size_t) i].relativePath == relativePath)
                currentIndex = 1 + getNumFactoryPresets() + i;
        return true;''',
            '''            if (userPresets[(size_t) i].relativePath == relativePath)
                currentIndex = 1 + getNumFactoryPresets() + i;
        markCurrentStateClean();
        return true;''',
        ),
    ],
    "MotionFX_source/Source/Tests/AudioIntegrityTest.cpp": [
        (
            '''    if (numFactory != 16) { std::cout << "  [FAIL] preset count mismatch" << std::endl; ++failures; }



    // 1) default state, a few sample rates / block sizes''',
            '''    if (numFactory != 16) { std::cout << "  [FAIL] preset count mismatch" << std::endl; ++failures; }

    // Init must be a genuinely clean starting point: every processing module and hidden sync toggle disabled.
    proc.presetManager.loadInitPreset();
    for (auto* id : { "drive", "pan", "volume", "space", "retro", "width" })
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

    proc.presetManager.loadInitPreset();

    // 1) default state, a few sample rates / block sizes''',
        ),
    ],
    "MotionFX_source/CHANGELOG.md": [
        (
            '''# MotionFX changelog

## 1.1.0-dev — Block 1

- Pause DSP when a host explicitly reports that its transport is stopped.
  - The dry input is left untouched.
  - Modulators, stutter capture and time-based/noise tails no longer continue in the background.
  - DSP state is reset once when playback transitions from running to stopped.
  - Hosts that expose no usable playhead continue to process normally.
- Preserve the selected effect identity when effect tabs are drag-reordered.
- Rename the master toggle from `MATCH` to `GAIN MATCH`.

## 0.2.0 — Block 2
- The preset name in the header is now the preset-browser control.
- Added an explicit Init preset that restores the unmodified default state.
- Added recursive user preset discovery in folders and subfolders.
- Added nested Factory Presets and User Presets menus.
- Added preset saving into a selected relative folder.
- Added preset-folder creation from the preset browser and options menu.
- Added preset-browser access from the options menu.
- Made JUCE acquisition portable through JUCE_DIR, a sibling checkout, or CMake FetchContent.
- Added GitHub Actions builds for Windows/MSVC and Linux, including automated audio and GUI tests.
''',
            '''# MotionFX changelog

## 0.3.0 — Block 3

- Matched the preset-name control height to the other header buttons.
- Removed the redundant hamburger preset-menu button.
- Fixed the dragged effect tab changing its displayed identity while crossing another slot.
- Enlarged modulation-source selectors and their popup text for readability.
- Made Init start with every effect module and hidden sync toggle disabled.
- Persisted the loaded preset identity in DAW project/session state.
- Added an asterisk when the current parameters differ from the loaded or saved preset.
- Limited knob value displays to at most three decimal places.
- Added double-click direct value entry on knobs.
- Added a scrollable About/resources window by clicking the MOTIONFX title.
- Added About, changelog and preset-folder shortcuts to the Options menu.
- Added descriptive tooltips to the header controls.
- Updated GitHub Actions to Node.js 24-compatible action versions.
- Added automated checks for clean Init state and preset-name/modified-state restoration.
- Updated the CMake/VST3 project version to `0.3.0` for Build 3.

## 0.2.0 — Block 2

- The preset name in the header is now the preset-browser control.
- Added an explicit Init preset that restores the unmodified default state.
- Added recursive user preset discovery in folders and subfolders.
- Added nested Factory Presets and User Presets menus.
- Added preset saving into a selected relative folder.
- Added preset-folder creation from the preset browser and options menu.
- Added preset-browser access from the options menu.
- Made JUCE acquisition portable through JUCE_DIR, a sibling checkout, or CMake FetchContent.
- Added GitHub Actions builds for Windows/MSVC and Linux, including automated audio and GUI tests.

## 0.1.0 — Block 1

- Pause DSP when a host explicitly reports that its transport is stopped.
  - The dry input is left untouched.
  - Modulators, stutter capture and time-based/noise tails no longer continue in the background.
  - DSP state is reset once when playback transitions from running to stopped.
  - Hosts that expose no usable playhead continue to process normally.
- Preserve the selected effect identity when effect tabs are drag-reordered.
- Rename the master toggle from `MATCH` to `GAIN MATCH`.
''',
        ),
    ],
}

# Read and validate everything before writing anything.
updated: dict[Path, str] = {}
errors: list[str] = []

for relative, replacements in changes.items():
    path = ROOT / relative
    if not path.is_file():
        errors.append(f"Missing file: {relative}")
        continue

    text = path.read_text(encoding="utf-8")
    for index, (old, new) in enumerate(replacements, start=1):
        count = text.count(old)
        if count == 0:
            errors.append(f"{relative}: expected fragment #{index} was not found")
            continue
        # Some intentionally global version replacements occur twice.
        if relative == ".github/workflows/build.yml" and old == "actions/checkout@v4":
            text = text.replace(old, new)
        else:
            if count != 1:
                errors.append(f"{relative}: fragment #{index} matched {count} times instead of once")
                continue
            text = text.replace(old, new, 1)
    updated[path] = text

if errors:
    print("Block 3 was NOT applied. No files were changed.\n", file=sys.stderr)
    for error in errors:
        print(f"- {error}", file=sys.stderr)
    sys.exit(1)

for path, text in updated.items():
    path.write_text(text, encoding="utf-8", newline="\n")

print(f"MotionFX Block 3 applied successfully to {len(updated)} files.")
