#pragma once
#include "LookAndFeel.h"
#include "../DSP/EffectChain.h"
#include "../Parameters.h"

namespace mfx
{
    // Tab index 0..5 = effect tabs shown in "order" sequence; index 6 = stutter (pinned last).
    class TabStrip : public juce::Component
    {
    public:
        static constexpr int stutterTabIndex = numEffects;

        std::function<void (int)> onSelect;
        std::function<void (std::array<EffectId, numEffects>)> onReorder;

        void setOrder (std::array<EffectId, numEffects> o) { order = o; repaint(); }
        void setSelectedSlot (int slot) { selectedSlot = slot; repaint(); }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (Palette::bg1);
            g.fillRoundedRectangle (b, 8.0f);

            int n = numEffects + 1;
            float tabW = b.getWidth() / (float) n;

            for (int visualSlot = 0; visualSlot < n; ++visualSlot)
            {
                bool isStutter = (visualSlot == numEffects);
                bool isDraggedTab = (! isStutter && dragging && visualSlot == dragCurrentSlot);
                if (isDraggedTab) continue; // drawn on top afterwards, following the mouse

                auto cell = juce::Rectangle<float> (b.getX() + visualSlot * tabW, b.getY(), tabW, b.getHeight()).reduced (3.0f);
                juce::Colour accent = isStutter ? Palette::red : Palette::effectColour ((int) order[(size_t) visualSlot]);
                juce::String label = isStutter ? "STUTTER" : effectDisplayNames[(int) order[(size_t) visualSlot]];
                bool selected = (visualSlot == selectedSlot);
                drawTab (g, cell, accent, label, selected);
            }

            if (dragging)
            {
                auto cell = juce::Rectangle<float> (dragMouseX - tabW * 0.5f, b.getY(), tabW, b.getHeight()).reduced (3.0f);
                juce::Colour accent = Palette::effectColour ((int) order[(size_t) dragStartSlot]);
                drawTab (g, cell, accent, effectDisplayNames[(int) order[(size_t) dragStartSlot]], true, true);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            int n = numEffects + 1;
            float tabW = (float) getWidth() / (float) n;
            int slot = juce::jlimit (0, n - 1, (int) (e.position.x / tabW));
            dragStartSlot = slot;
            dragCurrentSlot = slot;
            dragStartX = e.position.x;
            dragMouseX = e.position.x;
            dragging = false; // becomes true only once the mouse actually moves past a threshold
            movedEnough = false;
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragStartSlot >= numEffects) return; // stutter tab isn't reorderable
            dragMouseX = e.position.x;
            if (! movedEnough && std::abs (e.position.x - dragStartX) > 6.0f)
                movedEnough = true;
            if (! movedEnough) return;

            dragging = true;
            int n = numEffects;
            float tabW = (float) getWidth() / (float) (numEffects + 1);
            int target = juce::jlimit (0, n - 1, (int) (dragMouseX / tabW));
            if (target != dragCurrentSlot)
            {
                std::swap (order[(size_t) dragCurrentSlot], order[(size_t) target]);
                dragCurrentSlot = target;
                if (onReorder) onReorder (order);
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
        void drawTab (juce::Graphics& g, juce::Rectangle<float> cell, juce::Colour accent,
                      const juce::String& label, bool selected, bool floating = false)
        {
            g.setColour (selected ? Palette::panelHi : Palette::panel);
            g.fillRoundedRectangle (cell, 6.0f);
            g.setColour (floating ? accent.brighter (0.3f) : (selected ? accent : Palette::stroke));
            g.drawRoundedRectangle (cell, 6.0f, selected || floating ? 2.0f : 1.0f);

            if (selected || floating)
            {
                g.setColour (accent);
                g.fillRoundedRectangle (cell.getX(), cell.getBottom() - 3.0f, cell.getWidth(), 3.0f, 1.5f);
            }

            g.setColour (selected || floating ? Palette::text : Palette::textDim);
            g.setFont (juce::Font (juce::FontOptions (juce::jmin (14.0f, cell.getHeight() * 0.32f))).withStyle (juce::Font::bold));
            g.drawFittedText (label, cell.toNearestInt(), juce::Justification::centred, 1);
        }

        std::array<EffectId, numEffects> order { EffectId::Drive, EffectId::Retro, EffectId::Pan,
                                                  EffectId::Width, EffectId::Volume, EffectId::Space };
        int selectedSlot = 0;
        int dragStartSlot = 0, dragCurrentSlot = 0;
        float dragStartX = 0.0f, dragMouseX = 0.0f;
        bool dragging = false, movedEnough = false;
    };
}
