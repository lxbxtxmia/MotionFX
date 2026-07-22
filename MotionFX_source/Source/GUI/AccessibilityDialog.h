#pragma once
#include "LookAndFeel.h"
#include <functional>

namespace mfx
{
    class AccessibilityDialogContent final : public juce::Component
    {
    public:
        explicit AccessibilityDialogContent (
            std::function<void()> settingsChanged)
            : onSettingsChanged (std::move (settingsChanged))
        {
            configureOption (
                highContrast,
                highContrastDescription,
                "High contrast",
                "Strengthens text, borders and control separation on any selected theme.",
                UiPreferences::instance().isHighContrast());

            configureOption (
                reducedMotion,
                reducedMotionDescription,
                "Reduced motion",
                "Stops decorative waveform scrolling and removes non-essential visual movement.",
                UiPreferences::instance().isReducedMotion());

            configureOption (
                enhancedControls,
                enhancedControlsDescription,
                "Enhanced controls",
                "Uses thicker knob arcs, clearer pointers and stronger keyboard-focus rings.",
                UiPreferences::instance().hasEnhancedControls());

            configureOption (
                largerText,
                largerTextDescription,
                "Larger interface text",
                "Increases interface text by approximately 12% while preserving the layout.",
                UiPreferences::instance().hasLargerText());

            highContrast.onClick = [this]
            {
                UiPreferences::instance().setHighContrast (
                    highContrast.getToggleState());
                changed();
            };

            reducedMotion.onClick = [this]
            {
                UiPreferences::instance().setReducedMotion (
                    reducedMotion.getToggleState());
                changed();
            };

            enhancedControls.onClick = [this]
            {
                UiPreferences::instance().setEnhancedControls (
                    enhancedControls.getToggleState());
                changed();
            };

            largerText.onClick = [this]
            {
                UiPreferences::instance().setLargerText (
                    largerText.getToggleState());
                changed();
            };

            setSize (520, 350);
        }

        void paint (juce::Graphics& graphics) override
        {
            graphics.fillAll (Palette::bg0);

            graphics.setColour (Palette::text);
            graphics.setFont (FontBank::font (20.0f, true));
            graphics.drawText (
                "Accessibility",
                18, 12, getWidth() - 36, 30,
                juce::Justification::centredLeft);

            graphics.setColour (Palette::textDim);
            graphics.setFont (FontBank::font (11.5f));
            graphics.drawText (
                "These preferences are global and are not stored inside presets.",
                18, 42, getWidth() - 36, 24,
                juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (18);
            bounds.removeFromTop (58);

            layoutOption (
                highContrast,
                highContrastDescription,
                bounds.removeFromTop (66));
            bounds.removeFromTop (8);
            layoutOption (
                reducedMotion,
                reducedMotionDescription,
                bounds.removeFromTop (66));
            bounds.removeFromTop (8);
            layoutOption (
                enhancedControls,
                enhancedControlsDescription,
                bounds.removeFromTop (66));
            bounds.removeFromTop (8);
            layoutOption (
                largerText,
                largerTextDescription,
                bounds.removeFromTop (66));
        }

    private:
        void configureOption (
            juce::ToggleButton& toggle,
            juce::Label& description,
            const juce::String& title,
            const juce::String& explanation,
            bool enabled)
        {
            toggle.setButtonText (title);
            toggle.setToggleState (
                enabled,
                juce::dontSendNotification);
            addAndMakeVisible (toggle);

            description.setText (
                explanation,
                juce::dontSendNotification);
            description.setColour (
                juce::Label::textColourId,
                Palette::textDim);
            description.setFont (FontBank::font (11.0f));
            description.setJustificationType (
                juce::Justification::topLeft);
            description.setMinimumHorizontalScale (0.82f);
            addAndMakeVisible (description);
        }

        static void layoutOption (
            juce::ToggleButton& toggle,
            juce::Label& description,
            juce::Rectangle<int> area)
        {
            toggle.setBounds (
                area.removeFromTop (30));
            description.setBounds (
                area.reduced (8, 0));
        }

        void changed()
        {
            for (auto* label : {
                     &highContrastDescription,
                     &reducedMotionDescription,
                     &enhancedControlsDescription,
                     &largerTextDescription
                 })
            {
                label->setColour (
                    juce::Label::textColourId,
                    Palette::textDim);
            }

            sendLookAndFeelChange();
            repaint();

            if (onSettingsChanged)
                onSettingsChanged();
        }

        std::function<void()> onSettingsChanged;

        juce::ToggleButton highContrast;
        juce::ToggleButton reducedMotion;
        juce::ToggleButton enhancedControls;
        juce::ToggleButton largerText;

        juce::Label highContrastDescription;
        juce::Label reducedMotionDescription;
        juce::Label enhancedControlsDescription;
        juce::Label largerTextDescription;
    };
}
