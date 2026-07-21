#pragma once
#include "PluginProcessor.h"
#include "GUI/LookAndFeel.h"
#include "GUI/Widgets.h"
#include "GUI/EffectPanel.h"
#include "GUI/StutterPanel.h"
#include "GUI/TabStrip.h"

class MotionFXAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MotionFXAudioProcessorEditor (MotionFXAudioProcessor&);
    ~MotionFXAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void setScalePercent (int percent);
    void showPresetMenu();
    void showOptionsMenu();
    void showAboutDialog();
    void showChangelogDialog();
    void showScrollableTextDialog (const juce::String& title, const juce::String& text);
    void refreshPresetLabel();
    void savePresetDialog();
    void createPresetFolderDialog();
    void timerCallback() override;

    MotionFXAudioProcessor& processor;
    mfx::MotionFXLookAndFeel lookAndFeel;

    static constexpr int baseW = 1080, baseH = 720;
    int scalePercent = 100;

    juce::Component content; // fixed baseW x baseH logical canvas, scaled via AffineTransform

    // header
    juce::Label titleLabel;
    juce::TextButton presetNameButton;
    juce::TextButton prevPresetBtn { "<" }, nextPresetBtn { ">" };
    juce::TextButton savePresetBtn { "Save" }, optionsBtn { juce::CharPointer_UTF8 ("\xe2\x9a\x99") };

    std::unique_ptr<mfx::LabeledKnob> inputKnob, outputKnob, dryWetKnob;
    std::unique_ptr<mfx::LabeledToggle> matchGainToggle;

    mfx::TabStrip tabStrip;
    int selectedSlot = 0;

    std::array<std::unique_ptr<mfx::EffectPanel>, mfx::numEffects> effectPanels;
    std::unique_ptr<mfx::StutterPanel> stutterPanel;

    std::unique_ptr<juce::ComponentBoundsConstrainer> constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MotionFXAudioProcessorEditor)
};
