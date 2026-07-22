#pragma once
#include "PluginProcessor.h"
#include "GUI/LookAndFeel.h"
#include "GUI/Widgets.h"
#include "GUI/EffectPanel.h"
#include "GUI/StutterPanel.h"
#include "GUI/TabStrip.h"

class MotionFXAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
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
    void showAboutDialog (bool openChangelog = false);
    void showChangelogDialog();
    void refreshPresetLabel();
    void savePresetDialog();
    void createPresetFolderDialog();
    void showPresetLoadErrorIfAny();
    void timerCallback() override;

    MotionFXAudioProcessor& processor;
    mfx::MotionFXLookAndFeel lookAndFeel;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;

    static constexpr int baseW = 1080;
    static constexpr int baseH = 720;
    int scalePercent = 100;

    juce::Component content;

    juce::Label titleLabel;
    juce::TextButton presetNameButton;
    juce::TextButton prevPresetBtn { "<" };
    juce::TextButton nextPresetBtn { ">" };
    juce::TextButton undoBtn { "UNDO" };
    juce::TextButton redoBtn { "REDO" };
    juce::TextButton savePresetBtn { "Save" };
    juce::TextButton optionsBtn {
        juce::CharPointer_UTF8 ("\xe2\x9a\x99")
    };

    std::unique_ptr<mfx::LabeledKnob> inputKnob;
    std::unique_ptr<mfx::LabeledKnob> outputKnob;
    std::unique_ptr<mfx::LabeledKnob> dryWetKnob;
    std::unique_ptr<mfx::LabeledToggle> matchGainToggle;

    mfx::TabStrip tabStrip;
    int selectedSlot = 0;

    std::array<std::unique_ptr<mfx::EffectPanel>,
               mfx::numEffects> effectPanels;
    std::unique_ptr<mfx::StutterPanel> stutterPanel;

    std::unique_ptr<juce::ComponentBoundsConstrainer> constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (
        MotionFXAudioProcessorEditor)
};
