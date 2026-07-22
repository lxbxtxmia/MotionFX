#pragma once
#include "LookAndFeel.h"
#include "../DSP/EffectChain.h"
#include "../Parameters.h"

namespace mfx
{
    class TabStrip : public juce::Component
    {
    public:
        static constexpr int stutterTabIndex = numEffects;

        std::function<void (int)> onSelect;
        std::function<void (std::array<EffectId, numEffects>)> onReorder;

        void setOrder (std::array<EffectId, numEffects> newOrder)
        {
            order = newOrder;
            repaint();
        }

        void setSelectedSlot (int slot)
        {
            selectedSlot = slot;
            repaint();
        }

        void paint (juce::Graphics& graphics) override
        {
            const auto bounds = getLocalBounds().toFloat();
            graphics.setColour (Palette::bg1);
            graphics.fillRoundedRectangle (bounds, 8.0f);

            const int tabCount = numEffects + 1;
            const float tabWidth = bounds.getWidth() / (float) tabCount;

            for (int visualSlot = 0; visualSlot < tabCount; ++visualSlot)
            {
                const bool stutter = visualSlot == numEffects;
                const bool dragged = ! stutter && dragging && visualSlot == dragCurrentSlot;
                if (dragged)
                    continue;

                const auto cell = juce::Rectangle<float> (
                    bounds.getX() + visualSlot * tabWidth, bounds.getY(),
                    tabWidth, bounds.getHeight()).reduced (3.0f);

                const juce::Colour accent = stutter
                    ? Palette::red
                    : Palette::effectColour ((int) order[(size_t) visualSlot]);
                const juce::String label = stutter
                    ? "STUTTER"
                    : effectDisplayNames[(int) order[(size_t) visualSlot]];

                drawTab (graphics, cell, accent, label,
                         visualSlot == selectedSlot, false);
            }

            if (dragging)
            {
                const auto cell = juce::Rectangle<float> (
                    dragMouseX - tabWidth * 0.5f, bounds.getY(),
                    tabWidth, bounds.getHeight()).reduced (3.0f);
                const juce::Colour accent = Palette::effectColour ((int) draggedEffect);

                // Drag outline only: this must not look like a second selected tab.
                drawTab (graphics, cell, accent,
                         effectDisplayNames[(int) draggedEffect], false, true);
            }
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            const int tabCount = numEffects + 1;
            const float tabWidth = (float) getWidth() / (float) tabCount;
            const int slot = juce::jlimit (0, tabCount - 1,
                                           (int) (event.position.x / tabWidth));

            dragStartSlot = slot;
            dragCurrentSlot = slot;
            dragStartX = event.position.x;
            dragMouseX = event.position.x;
            dragging = false;
            movedEnough = false;

            if (slot < numEffects)
                draggedEffect = order[(size_t) slot];
        }

        void mouseDrag (const juce::MouseEvent& event) override
        {
            if (dragStartSlot >= numEffects)
                return;

            dragMouseX = event.position.x;
            if (! movedEnough && std::abs (event.position.x - dragStartX) > 6.0f)
                movedEnough = true;
            if (! movedEnough)
                return;

            dragging = true;
            const float tabWidth = (float) getWidth() / (float) (numEffects + 1);
            const int target = juce::jlimit (0, numEffects - 1,
                                             (int) (dragMouseX / tabWidth));

            if (target != dragCurrentSlot)
            {
                std::swap (order[(size_t) dragCurrentSlot], order[(size_t) target]);
                dragCurrentSlot = target;
                if (onReorder)
                    onReorder (order);
            }

            repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (! movedEnough && onSelect)
                onSelect (dragStartSlot);

            dragging = false;
            movedEnough = false;
            repaint();
        }

    private:
        void drawTab (juce::Graphics& graphics,
                      juce::Rectangle<float> cell,
                      juce::Colour accent,
                      const juce::String& label,
                      bool selected,
                      bool floating)
        {
            graphics.setColour (selected ? Palette::panelHi
                                         : floating ? Palette::panelHi.withAlpha (0.88f)
                                                    : Palette::panel);
            graphics.fillRoundedRectangle (cell, 6.0f);

            graphics.setColour (floating ? accent.withAlpha (0.85f)
                                         : selected ? accent : Palette::stroke);
            graphics.drawRoundedRectangle (cell, 6.0f,
                                            selected || floating ? 2.0f : 1.0f);

            if (selected)
            {
                graphics.setColour (accent);
                graphics.fillRoundedRectangle (cell.getX(), cell.getBottom() - 3.0f,
                                                cell.getWidth(), 3.0f, 1.5f);
            }

            graphics.setColour (selected || floating ? Palette::text : Palette::textDim);
            graphics.setFont (juce::Font (juce::FontOptions (
                juce::jmin (14.0f, cell.getHeight() * 0.32f))).withStyle (juce::Font::bold));
            graphics.drawFittedText (label, cell.toNearestInt(),
                                     juce::Justification::centred, 1);
        }

        std::array<EffectId, numEffects> order {
            EffectId::Drive, EffectId::Retro, EffectId::Filter, EffectId::Pan,
            EffectId::Width, EffectId::Volume, EffectId::Space
        };

        int selectedSlot = 0;
        int dragStartSlot = 0, dragCurrentSlot = 0;
        float dragStartX = 0.0f, dragMouseX = 0.0f;
        bool dragging = false, movedEnough = false;
        EffectId draggedEffect = EffectId::Drive;
    };
}
