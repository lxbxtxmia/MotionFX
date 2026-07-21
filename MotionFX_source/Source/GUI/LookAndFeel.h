#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mfx
{
    namespace Palette
    {
        static const juce::Colour bg0      { 0xff14141c };
        static const juce::Colour bg1      { 0xff1c1c26 };
        static const juce::Colour panel    { 0xff22222e };
        static const juce::Colour panelHi  { 0xff2b2b3a };
        static const juce::Colour stroke   { 0xff3a3a4a };
        static const juce::Colour text     { 0xffe8e8f0 };
        static const juce::Colour textDim  { 0xff8a8a9a };
        static const juce::Colour teal     { 0xff36e0c8 };
        static const juce::Colour purple   { 0xffb06bff };
        static const juce::Colour pink     { 0xffff5fa8 };
        static const juce::Colour orange   { 0xffffa94d };
        static const juce::Colour yellow   { 0xfff5e15c };
        static const juce::Colour red      { 0xffff5a5a };

        inline juce::Colour effectColour (int effectIndex)
        {
            switch (effectIndex)
            {
                case 0: return orange; // Drive
                case 1: return teal;   // Pan
                case 2: return yellow; // Volume
                case 3: return purple; // Space
                case 4: return pink;   // Retro
                case 5: return juce::Colour (0xff6fa8ff); // Width
                default: return teal;
            }
        }
    }

    class MotionFXLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        MotionFXLookAndFeel()
        {
            setColour (juce::ResizableWindow::backgroundColourId, Palette::bg0);
            setColour (juce::Slider::textBoxTextColourId, Palette::text);
            setColour (juce::ComboBox::textColourId, Palette::text);
            setColour (juce::ComboBox::backgroundColourId, Palette::panel);
            setColour (juce::PopupMenu::backgroundColourId, Palette::panelHi);
            setColour (juce::PopupMenu::textColourId, Palette::text);
            setColour (juce::TextButton::textColourOffId, Palette::text);
            setColour (juce::Label::textColourId, Palette::text);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                juce::Slider& slider) override
        {
            auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (width * 0.08f);
            auto centre = bounds.getCentre();
            float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            juce::Colour accent = slider.findColour (juce::Slider::rotarySliderFillColourId, true);
            if (accent == juce::Colours::transparentBlack) accent = Palette::teal;

            // outer track
            juce::Path track;
            track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
            g.setColour (Palette::stroke);
            g.strokePath (track, juce::PathStrokeType (radius * 0.16f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // value arc
            juce::Path valueArc;
            valueArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, angle, true);
            g.setColour (accent);
            g.strokePath (valueArc, juce::PathStrokeType (radius * 0.16f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // glossy knob body
            float bodyR = radius * 0.62f;
            juce::ColourGradient grad (Palette::panelHi.brighter (0.15f), centre.x, centre.y - bodyR,
                                        Palette::panel.darker (0.3f), centre.x, centre.y + bodyR, false);
            g.setGradientFill (grad);
            g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
            g.setColour (Palette::stroke);
            g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);

            // pointer
            juce::Path pointer;
            float pointerLen = bodyR * 0.82f;
            pointer.addRoundedRectangle (-radius * 0.045f, -pointerLen, radius * 0.09f, pointerLen * 0.62f, radius * 0.04f);
            g.setColour (accent.brighter (0.4f));
            g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
        }

        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
        {
            juce::ignoreUnused (shouldDrawButtonAsDown);
            auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);

            bool isPillStyle = button.getButtonText().isEmpty();
            if (isPillStyle)
            {
                float radius = bounds.getHeight() * 0.5f;
                g.setColour (button.getToggleState() ? Palette::teal.withAlpha (0.85f) : Palette::stroke);
                g.fillRoundedRectangle (bounds, radius);
                float knobD = bounds.getHeight() - 4.0f;
                float knobX = button.getToggleState() ? bounds.getRight() - knobD - 2.0f : bounds.getX() + 2.0f;
                g.setColour (Palette::text);
                g.fillEllipse (knobX, bounds.getY() + 2.0f, knobD, knobD);
            }
            else
            {
                g.setColour (shouldDrawButtonAsHighlighted ? Palette::panelHi : Palette::panel);
                g.fillRoundedRectangle (bounds, 4.0f);
                g.setColour (button.getToggleState() ? Palette::teal : Palette::stroke);
                g.drawRoundedRectangle (bounds, 4.0f, 1.2f);
                g.setColour (button.getToggleState() ? Palette::teal : Palette::textDim);
                g.setFont (juce::Font (juce::FontOptions (bounds.getHeight() * 0.55f)));
                g.drawFittedText (button.getButtonText(), bounds.toNearestInt(), juce::Justification::centred, 1);
            }
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
        {
            auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
            juce::Colour base = shouldDrawButtonAsDown ? Palette::panelHi.brighter (0.1f)
                               : shouldDrawButtonAsHighlighted ? Palette::panelHi : Palette::panel;
            g.setColour (base);
            g.fillRoundedRectangle (bounds, 5.0f);
            g.setColour (Palette::stroke);
            g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
        }

        void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
        {
            auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (1.0f);
            g.setColour (Palette::panel);
            g.fillRoundedRectangle (bounds, 4.0f);
            g.setColour (box.hasKeyboardFocus (true) ? Palette::teal : Palette::stroke);
            g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

            juce::Path arrow;
            float ax = width - 14.0f, ay = height * 0.5f;
            arrow.addTriangle (ax - 4.0f, ay - 2.5f, ax + 4.0f, ay - 2.5f, ax, ay + 3.0f);
            g.setColour (Palette::textDim);
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

        juce::Font getLabelFont (juce::Label& l) override
        {
            return juce::Font (juce::FontOptions (juce::jmax (10.0f, l.getHeight() * 0.62f)));
        }

        void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool, bool) override {}
    };
}
