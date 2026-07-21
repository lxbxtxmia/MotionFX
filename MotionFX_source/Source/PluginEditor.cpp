#include "PluginEditor.h"
#include "GUI/LookAndFeel.h"

using namespace mfx;

namespace
{
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
                s.secondaryKnobs = { { "drive_tone", "TONE" }, { "drive_mix", "MIX" }, { "drive_outtrim", "OUT" } };
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
                s.secondaryKnobs = { { "retro_rate", "RATE" }, { "retro_tone", "TONE" }, { "retro_mix", "MIX" } };
                break;
            case EffectId::Width:
                s.secondaryKnobs = { { "width_crossover", "X-OVER" } };
                break;
        }
        return s;
    }
}

MotionFXAudioProcessorEditor::MotionFXAudioProcessorEditor (MotionFXAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);

    titleLabel.setText ("MOTIONFX", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (22.0f)).withStyle (juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, Palette::teal);
    content.addAndMakeVisible (titleLabel);

    presetNameButton.setClickingTogglesState (false);
    presetNameButton.setTooltip ("Open preset browser");
    content.addAndMakeVisible (presetNameButton);
    presetNameButton.onClick = [this] { showPresetMenu(); };

    for (auto* btn : { &prevPresetBtn, &nextPresetBtn, &presetMenuBtn, &savePresetBtn, &optionsBtn })
        content.addAndMakeVisible (btn);

    prevPresetBtn.onClick = [this] { processor.presetManager.previous(); refreshPresetLabel(); };
    nextPresetBtn.onClick = [this] { processor.presetManager.next(); refreshPresetLabel(); };
    presetMenuBtn.onClick = [this] { showPresetMenu(); };
    savePresetBtn.onClick = [this] { savePresetDialog(); };
    optionsBtn.onClick = [this] { showOptionsMenu(); };

    inputKnob = std::make_unique<LabeledKnob> (processor.apvts, "master_input", "INPUT", Palette::teal);
    outputKnob = std::make_unique<LabeledKnob> (processor.apvts, "master_output", "OUTPUT", Palette::teal);
    dryWetKnob = std::make_unique<LabeledKnob> (processor.apvts, "master_drywet", "DRY/WET", Palette::purple);
    matchGainToggle = std::make_unique<LabeledToggle> (processor.apvts, "master_matchgain", "GAIN MATCH");
    for (auto* c : { (juce::Component*) inputKnob.get(), (juce::Component*) outputKnob.get(),
                     (juce::Component*) dryWetKnob.get(), (juce::Component*) matchGainToggle.get() })
        content.addAndMakeVisible (c);

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

    for (int e = 0; e < numEffects; ++e)
    {
        effectPanels[(size_t) e] = std::make_unique<EffectPanel> (processor.apvts, processor.chain, makeSpec ((EffectId) e), (EffectId) e);
        content.addAndMakeVisible (*effectPanels[(size_t) e]);
    }
    stutterPanel = std::make_unique<StutterPanel> (processor.apvts, processor.chain);
    content.addAndMakeVisible (*stutterPanel);

    constrainer = std::make_unique<juce::ComponentBoundsConstrainer>();
    constrainer->setFixedAspectRatio ((double) baseW / (double) baseH);
    constrainer->setSizeLimits (baseW / 4, baseH / 4, baseW * 3, baseH * 3);
    setConstrainer (constrainer.get());
    setResizable (true, true);

    refreshPresetLabel();
    setScalePercent (100);
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

void MotionFXAudioProcessorEditor::setScalePercent (int percent)
{
    scalePercent = juce::jlimit (25, 300, percent);
    float scale = scalePercent / 100.0f;
    content.setTransform (juce::AffineTransform::scale (scale));
    setSize ((int) (baseW * scale), (int) (baseH * scale));
}

void MotionFXAudioProcessorEditor::resized()
{
    // keep the transform in sync whenever the outer window changes size --
    // covers both corner-drag resizing and the discrete scale menu.
    float scale = getWidth() / (float) baseW;
    scalePercent = juce::roundToInt (scale * 100.0f);
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, baseW, baseH);

    auto b = content.getLocalBounds().reduced (14);

    auto header = b.removeFromTop (58);
    titleLabel.setBounds (header.removeFromLeft (170));

    auto masterArea = header.removeFromRight (330);
    matchGainToggle->setBounds (masterArea.removeFromRight (70).reduced (0, 14));
    dryWetKnob->setBounds (masterArea.removeFromRight (86));
    outputKnob->setBounds (masterArea.removeFromRight (86));
    inputKnob->setBounds (masterArea.removeFromRight (86));

    auto presetBar = header;
    optionsBtn.setBounds (presetBar.removeFromRight (36).reduced (2, 10));
    presetBar.removeFromRight (4);
    savePresetBtn.setBounds (presetBar.removeFromRight (56).reduced (0, 12));
    presetBar.removeFromRight (4);
    nextPresetBtn.setBounds (presetBar.removeFromRight (28).reduced (0, 10));
    presetMenuBtn.setBounds (presetBar.removeFromRight (28).reduced (0, 10));
    prevPresetBtn.setBounds (presetBar.removeFromLeft (28).reduced (0, 10));
    presetNameButton.setBounds (presetBar);

    b.removeFromTop (10);
    tabStrip.setBounds (b.removeFromTop (40));
    b.removeFromTop (10);

    auto order = processor.getOrder();
    for (int slot = 0; slot < numEffects; ++slot)
    {
        bool visible = (slot == selectedSlot);
        auto* panel = effectPanels[(size_t) order[(size_t) slot]].get();
        panel->setVisible (visible);
        if (visible) panel->setBounds (b);
    }
    bool stutterVisible = (selectedSlot == numEffects);
    stutterPanel->setVisible (stutterVisible);
    if (stutterVisible) stutterPanel->setBounds (b);
}

void MotionFXAudioProcessorEditor::refreshPresetLabel()
{
    presetNameButton.setButtonText (processor.presetManager.getCurrentName());
}

void MotionFXAudioProcessorEditor::timerCallback()
{
    tabStrip.setOrder (processor.getOrder());
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
            processor.presetManager.loadByCombinedIndex (result - 1);
            refreshPresetLabel();
        }
        else if (result == 9001) savePresetDialog();
        else if (result == 9002) createPresetFolderDialog();
    });
}

void MotionFXAudioProcessorEditor::savePresetDialog()
{
    auto aw = std::make_shared<juce::AlertWindow> ("Save Preset",
        "Enter a name and an optional folder path (for example User Made/Drums):", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", processor.presetManager.getCurrentName() == "Init" ? "New Preset" : processor.presetManager.getCurrentName());
    aw->addTextEditor ("folder", "User Made");
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int result) mutable
    {
        if (result == 1)
        {
            auto name = aw->getTextEditorContents ("name").trim();
            auto folder = aw->getTextEditorContents ("folder").trim();
            if (name.isNotEmpty() && processor.presetManager.saveUserPreset (name, folder))
                refreshPresetLabel();
        }
        aw.reset();
    }));
}

void MotionFXAudioProcessorEditor::createPresetFolderDialog()
{
    auto aw = std::make_shared<juce::AlertWindow> ("Create Preset Folder",
        "Folder path relative to the preset library:", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("folder", "User Made");
    aw->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int result) mutable
    {
        if (result == 1)
            processor.presetManager.createPresetFolder (aw->getTextEditorContents ("folder").trim());
        aw.reset();
    }));
}

void MotionFXAudioProcessorEditor::showOptionsMenu()
{
    juce::PopupMenu menu;
    juce::PopupMenu scaleMenu;
    for (int pct : { 25, 50, 75, 100, 150, 200, 300 })
        scaleMenu.addItem (1000 + pct, juce::String (pct) + "%", true, scalePercent == pct);
    menu.addSubMenu ("Interface Scale", scaleMenu);
    menu.addItem (3, "Open Preset Browser...");
    menu.addItem (4, "Save Preset...");
    menu.addItem (5, "Create Preset Folder...");
    menu.addSeparator();
    menu.addItem (1, "Choose Preset Folder...");
    menu.addItem (2, "Reset Preset Folder to Default");

    menu.showMenuAsync (juce::PopupMenu::Options(), [this] (int result)
    {
        if (result >= 1000)
        {
            setScalePercent (result - 1000);
        }
        else if (result == 3) showPresetMenu();
        else if (result == 4) savePresetDialog();
        else if (result == 5) createPresetFolderDialog();
        else if (result == 1)
        {
            auto chooser = std::make_shared<juce::FileChooser> ("Choose a folder for MotionFX presets",
                                                                  processor.presetManager.getPresetDirectory());
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                [this, chooser] (const juce::FileChooser& fc)
                {
                    auto dir = fc.getResult();
                    if (dir != juce::File())
                        processor.presetManager.setPresetDirectory (dir);
                });
        }
        else if (result == 2)
        {
            processor.presetManager.setPresetDirectory (mfx::PresetManager::getDefaultPresetDirectory());
        }
    });
}
