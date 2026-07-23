#pragma once
#include "Theme.h"
#include "ValueFormatting.h"

namespace mfx
{
    class MotionFXLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        MotionFXLookAndFeel()
        {
            refreshColours();
        }

        void refreshColours()
        {
            UiPreferences::instance().applyTheme();

            setDefaultSansSerifTypeface (FontBank::regularTypeface());

            setColour (juce::ResizableWindow::backgroundColourId, Palette::bg0);
            setColour (juce::Slider::textBoxTextColourId, Palette::text);
            setColour (juce::Slider::textBoxBackgroundColourId,
                       juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxOutlineColourId,
                       juce::Colours::transparentBlack);
            setColour (juce::ComboBox::textColourId, Palette::text);
            setColour (juce::ComboBox::backgroundColourId, Palette::panel);
            setColour (juce::ComboBox::outlineColourId, Palette::stroke);
            setColour (juce::PopupMenu::backgroundColourId, Palette::panelHi);
            setColour (juce::PopupMenu::textColourId, Palette::text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId,
                       Palette::teal.withAlpha (0.22f));
            setColour (juce::PopupMenu::highlightedTextColourId, Palette::text);
            setColour (juce::TextButton::textColourOffId, Palette::text);
            setColour (juce::TextButton::textColourOnId, Palette::text);
            setColour (juce::Label::textColourId, Palette::text);
            setColour (juce::TextEditor::backgroundColourId, Palette::bg1);
            setColour (juce::TextEditor::textColourId, Palette::text);
            setColour (juce::TextEditor::outlineColourId, Palette::stroke);
            setColour (juce::TextEditor::focusedOutlineColourId, Palette::teal);
            setColour (juce::AlertWindow::backgroundColourId, Palette::bg1);
            setColour (juce::AlertWindow::textColourId, Palette::text);
            setColour (juce::AlertWindow::outlineColourId, Palette::stroke);
        }

        juce::Typeface::Ptr getTypefaceForFont (
            const juce::Font& font) override
        {
            auto typeface = font.isBold()
                ? FontBank::boldTypeface()
                : FontBank::regularTypeface();

            return typeface != nullptr
                ? typeface
                : juce::LookAndFeel_V4::getTypefaceForFont (font);
        }

        static float adaptiveCornerRadius (
            juce::Rectangle<float> bounds) noexcept
        {
            const float shortest = juce::jmin (
                bounds.getWidth(),
                bounds.getHeight());
            const float longest = juce::jmax (
                bounds.getWidth(),
                bounds.getHeight());
            const float ratio = longest / juce::jmax (1.0f, shortest);

            if (ratio < 1.28f)
                return juce::jlimit (3.0f, 4.5f, shortest * 0.11f);

            return juce::jlimit (4.5f, 7.5f, shortest * 0.16f);
        }

        void drawRotarySlider (juce::Graphics& graphics,
                               int x, int y, int width, int height,
                               float sliderPosition,
                               float rotaryStartAngle,
                               float rotaryEndAngle,
                               juce::Slider& slider) override
        {
            const auto available = juce::Rectangle<float> (
                (float) x, (float) y,
                (float) width, (float) height);

            const float size = juce::jmax (
                18.0f,
                juce::jmin (available.getWidth(),
                            available.getHeight()) * 0.90f);
            const auto bounds = available
                .withSizeKeepingCentre (size, size)
                .reduced (2.0f);

            const auto centre = bounds.getCentre();
            const float radius = bounds.getWidth() * 0.5f;
            const float angle = rotaryStartAngle
                + sliderPosition * (rotaryEndAngle - rotaryStartAngle);

            auto accent = slider.findColour (
                juce::Slider::rotarySliderFillColourId, true);

            if (accent == juce::Colours::transparentBlack)
                accent = Palette::teal;

            const bool enhanced =
                UiPreferences::instance().hasEnhancedControls();
            const float trackThickness = enhanced
                ? juce::jmax (3.4f, radius * 0.15f)
                : juce::jmax (2.4f, radius * 0.10f);

            juce::Path track;
            track.addCentredArc (
                centre.x, centre.y,
                radius - trackThickness * 0.5f,
                radius - trackThickness * 0.5f,
                0.0f,
                rotaryStartAngle,
                rotaryEndAngle,
                true);

            graphics.setColour (
                Palette::stroke.withAlpha (
                    Palette::isLight ? 0.72f : 0.82f));
            graphics.strokePath (
                track,
                juce::PathStrokeType (
                    trackThickness,
                    juce::PathStrokeType::curved,
                    juce::PathStrokeType::rounded));

            juce::Path valueArc;
            valueArc.addCentredArc (
                centre.x, centre.y,
                radius - trackThickness * 0.5f,
                radius - trackThickness * 0.5f,
                0.0f,
                rotaryStartAngle,
                angle,
                true);

            graphics.setColour (accent);
            graphics.strokePath (
                valueArc,
                juce::PathStrokeType (
                    trackThickness,
                    juce::PathStrokeType::curved,
                    juce::PathStrokeType::rounded));

            if (auto* provider =
                    dynamic_cast<ModulationDisplayProvider*> (&slider))
            {
                float minimum = 0.0f;
                float maximum = 0.0f;
                float current = 0.0f;

                if (provider->getModulationDisplay (
                        minimum, maximum, current))
                {
                    const float minimumAngle = rotaryStartAngle
                        + minimum
                            * (rotaryEndAngle - rotaryStartAngle);
                    const float maximumAngle = rotaryStartAngle
                        + maximum
                            * (rotaryEndAngle - rotaryStartAngle);
                    const float currentAngle = rotaryStartAngle
                        + current
                            * (rotaryEndAngle - rotaryStartAngle);

                    juce::Path rangeArc;
                    const float rangeRadius =
                        radius + (enhanced ? 1.2f : 0.5f);

                    rangeArc.addCentredArc (
                        centre.x,
                        centre.y,
                        rangeRadius,
                        rangeRadius,
                        0.0f,
                        minimumAngle,
                        maximumAngle,
                        true);

                    graphics.setColour (
                        accent.withAlpha (
                            enhanced ? 0.46f : 0.32f));
                    graphics.strokePath (
                        rangeArc,
                        juce::PathStrokeType (
                            enhanced ? 2.6f : 1.8f,
                            juce::PathStrokeType::curved,
                            juce::PathStrokeType::rounded));

                    const auto marker = centre
                        + juce::Point<float> (
                            std::sin (currentAngle),
                            -std::cos (currentAngle))
                          * rangeRadius;

                    graphics.setColour (
                        Palette::text.withAlpha (0.92f));
                    graphics.fillEllipse (
                        marker.x - 2.4f,
                        marker.y - 2.4f,
                        4.8f,
                        4.8f);
                    graphics.setColour (accent);
                    graphics.drawEllipse (
                        marker.x - 2.4f,
                        marker.y - 2.4f,
                        4.8f,
                        4.8f,
                        1.0f);
                }
            }

            const float bodyRadius = radius
                - trackThickness
                - (enhanced ? 5.0f : 4.0f);

            graphics.setColour (Palette::panelHi);
            graphics.fillEllipse (
                centre.x - bodyRadius,
                centre.y - bodyRadius,
                bodyRadius * 2.0f,
                bodyRadius * 2.0f);

            graphics.setColour (
                Palette::stroke.withAlpha (
                    enhanced ? 1.0f : 0.72f));
            graphics.drawEllipse (
                centre.x - bodyRadius,
                centre.y - bodyRadius,
                bodyRadius * 2.0f,
                bodyRadius * 2.0f,
                enhanced ? 1.5f : 1.0f);

            const float pointerLength = bodyRadius * 0.72f;
            juce::Path pointer;
            pointer.addRoundedRectangle (
                -1.5f,
                -pointerLength,
                3.0f,
                pointerLength * 0.58f,
                1.5f);

            graphics.setColour (
                enhanced ? accent.brighter (0.25f) : accent);
            graphics.fillPath (
                pointer,
                juce::AffineTransform::rotation (angle)
                    .translated (centre));

            if (slider.hasKeyboardFocus (true))
            {
                graphics.setColour (
                    Palette::text.withAlpha (0.90f));
                graphics.drawEllipse (
                    bounds.expanded (2.0f),
                    enhanced ? 2.0f : 1.2f);
            }
        }

        void drawToggleButton (juce::Graphics& graphics,
                               juce::ToggleButton& button,
                               bool highlighted,
                               bool buttonDown) override
        {
            juce::ignoreUnused (buttonDown);
            auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
            const float radius = adaptiveCornerRadius (bounds);

            const bool pillStyle = button.getButtonText().isEmpty();

            if (pillStyle)
            {
                const float pillRadius = bounds.getHeight() * 0.5f;
                graphics.setColour (
                    button.getToggleState()
                        ? Palette::teal
                        : Palette::stroke);
                graphics.fillRoundedRectangle (bounds, pillRadius);

                const float diameter = bounds.getHeight() - 4.0f;
                const float knobX = button.getToggleState()
                    ? bounds.getRight() - diameter - 2.0f
                    : bounds.getX() + 2.0f;

                graphics.setColour (
                    Palette::isLight
                        ? juce::Colours::white
                        : Palette::text);
                graphics.fillEllipse (
                    knobX,
                    bounds.getY() + 2.0f,
                    diameter,
                    diameter);
                return;
            }

            graphics.setColour (
                highlighted ? Palette::panelHi : Palette::panel);
            graphics.fillRoundedRectangle (bounds, radius);

            graphics.setColour (
                button.getToggleState()
                    ? Palette::teal
                    : Palette::stroke);
            graphics.drawRoundedRectangle (
                bounds,
                radius,
                button.getToggleState() ? 1.8f : 1.0f);

            graphics.setColour (
                button.getToggleState()
                    ? Palette::teal
                    : Palette::textDim);

            const float fontSize = juce::jlimit (
                12.5f,
                16.0f,
                bounds.getHeight() * 0.39f);

            graphics.setFont (FontBank::font (fontSize, true));
            graphics.drawFittedText (
                button.getButtonText(),
                bounds.reduced (7.0f, 3.0f).toNearestInt(),
                juce::Justification::centred,
                1,
                0.82f);
        }

        void drawButtonBackground (
            juce::Graphics& graphics,
            juce::Button& button,
            const juce::Colour&,
            bool highlighted,
            bool down) override
        {
            auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
            const float radius = adaptiveCornerRadius (bounds);

            const auto base = down
                ? Palette::panelHi.brighter (
                    Palette::isLight ? 0.02f : 0.08f)
                : highlighted
                    ? Palette::panelHi
                    : Palette::panel;

            graphics.setColour (base);
            graphics.fillRoundedRectangle (bounds, radius);

            graphics.setColour (
                button.hasKeyboardFocus (true)
                    ? Palette::teal
                    : Palette::stroke);
            graphics.drawRoundedRectangle (
                bounds,
                radius,
                button.hasKeyboardFocus (true) ? 1.8f : 1.0f);
        }

        void drawComboBox (juce::Graphics& graphics,
                           int width,
                           int height,
                           bool,
                           int, int, int, int,
                           juce::ComboBox& box) override
        {
            auto bounds = juce::Rectangle<float> (
                              0.0f, 0.0f,
                              (float) width,
                              (float) height)
                              .reduced (1.0f);
            const float radius = adaptiveCornerRadius (bounds);

            graphics.setColour (Palette::panel);
            graphics.fillRoundedRectangle (bounds, radius);

            graphics.setColour (
                box.hasKeyboardFocus (true)
                    ? Palette::teal
                    : Palette::stroke);
            graphics.drawRoundedRectangle (
                bounds,
                radius,
                box.hasKeyboardFocus (true) ? 1.8f : 1.0f);

            juce::Path arrow;
            const float centreX = width - 14.0f;
            const float centreY = height * 0.5f;
            arrow.addTriangle (
                centreX - 4.0f, centreY - 2.0f,
                centreX + 4.0f, centreY - 2.0f,
                centreX, centreY + 3.0f);

            graphics.setColour (Palette::textDim);
            graphics.fillPath (arrow);
        }

        juce::Font getLabelFont (juce::Label& label) override
        {
            const float requested = label.getFont().getHeight();
            const float maximum = juce::jmax (
                12.0f,
                label.getHeight() * 0.56f);
            const float height = juce::jlimit (
                12.5f,
                maximum,
                requested);

            return FontBank::font (
                height,
                label.getFont().isBold());
        }

        juce::Font getComboBoxFont (
            juce::ComboBox& box) override
        {
            return FontBank::font (
                juce::jlimit (
                    14.5f,
                    18.5f,
                    box.getHeight() * 0.45f));
        }

        juce::Font getPopupMenuFont() override
        {
            return FontBank::font (16.0f);
        }

        juce::Font getTextButtonFont (
            juce::TextButton& button,
            int buttonHeight) override
        {
            const float maximum =
                button.getButtonText().length() >= 10
                    ? 14.0f
                    : 16.5f;

            return FontBank::font (
                juce::jlimit (
                    12.5f,
                    maximum,
                    buttonHeight * 0.42f),
                false);
        }

        void drawTabButton (
            juce::TabBarButton&,
            juce::Graphics&,
            bool,
            bool) override
        {
        }
    };
}
