#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"
#include "../DSP/EffectChain.h"
#include "../Parameters.h"
#include <deque>

namespace mfx
{
    //==============================================================================
    class LabeledKnob : public juce::Component
    {
    public:
        LabeledKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
                     const juce::String& labelText, juce::Colour accent)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
            slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
            slider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
            slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            addAndMakeVisible (slider);

            label.setText (labelText, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setColour (juce::Label::textColourId, Palette::textDim);
            addAndMakeVisible (label);

            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            label.setBounds (b.removeFromBottom (16));
            slider.setBounds (b);
        }

        juce::Slider slider;
    private:
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    //==============================================================================
    class LabeledCombo : public juce::Component
    {
    public:
        LabeledCombo (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& labelText)
        {
            combo.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (combo);
            label.setText (labelText, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setColour (juce::Label::textColourId, Palette::textDim);
            addAndMakeVisible (label);

            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramId)))
            {
                combo.addItemList (p->choices, 1);
            }
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramId, combo);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            if (label.getText().isNotEmpty())
                label.setBounds (b.removeFromTop (14));
            combo.setBounds (b);
        }

        juce::ComboBox combo;
    private:
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    //==============================================================================
    class LabeledToggle : public juce::Component
    {
    public:
        LabeledToggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& labelText)
        {
            button.setButtonText (labelText);
            addAndMakeVisible (button);
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramId, button);
        }
        void resized() override { button.setBounds (getLocalBounds()); }
        juce::ToggleButton button;
    private:
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    //==============================================================================
    // Scrolling live trace of the effect's final modulated parameter value -- one
    // uniform visual language that works for LFO, envelope, motion & sequencer alike.
    class ModVisualizer : public juce::Component, private juce::Timer
    {
    public:
        ModVisualizer (std::atomic<float>& valueSource, juce::Colour accentColour)
            : source (valueSource), accent (accentColour)
        {
            history.assign (160, 0.0f);
            startTimerHz (30);
        }

        void setActive (bool a) { active = a; }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (Palette::bg1);
            g.fillRoundedRectangle (b, 6.0f);
            g.setColour (Palette::stroke);
            g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);

            // centre gridline
            g.setColour (Palette::stroke.withAlpha (0.5f));
            g.drawHorizontalLine ((int) (b.getCentreY()), b.getX() + 4, b.getRight() - 4);

            if (! active)
            {
                g.setColour (Palette::textDim);
                g.setFont (juce::Font (juce::FontOptions (juce::jmin (14.0f, b.getHeight() * 0.28f))));
                g.drawFittedText ("No Modulation", b.toNearestInt(), juce::Justification::centred, 1);
                return;
            }

            juce::Path p;
            auto inner = b.reduced (4.0f);
            for (size_t i = 0; i < history.size(); ++i)
            {
                float x = inner.getX() + inner.getWidth() * ((float) i / (float) (history.size() - 1));
                float y = inner.getBottom() - history[i] * inner.getHeight();
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (accent);
            g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            juce::Path fill (p);
            fill.lineTo (inner.getRight(), inner.getBottom());
            fill.lineTo (inner.getX(), inner.getBottom());
            fill.closeSubPath();
            g.setColour (accent.withAlpha (0.15f));
            g.fillPath (fill);

            float last = history.back();
            g.setColour (accent);
            g.fillEllipse (inner.getRight() - 4.0f, inner.getBottom() - last * inner.getHeight() - 4.0f, 8.0f, 8.0f);
        }

    private:
        void timerCallback() override
        {
            history.pop_front();
            history.push_back (juce::jlimit (0.0f, 1.0f, source.load (std::memory_order_relaxed)));
            repaint();
        }

        std::atomic<float>& source;
        juce::Colour accent;
        std::deque<float> history;
        bool active = false;
    };

    //==============================================================================
    // Draggable bar-graph editor for the step sequencer (bound directly to
    // <prefix>_seq_step0..31 float params -- no per-step attachments needed).
    class StepBarGrid : public juce::Component
    {
    public:
        StepBarGrid (juce::AudioProcessorValueTreeState& s, juce::String pfx, juce::Colour accentColour)
            : apvts (s), prefix (std::move (pfx)), accent (accentColour) {}

        void setNumSteps (int n) { numSteps = juce::jlimit (1, StepSequencer::maxSteps, n); repaint(); }
        void setCurrentStepProvider (std::function<int()> fn) { currentStepFn = std::move (fn); }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (Palette::bg1);
            g.fillRoundedRectangle (b, 6.0f);

            float cellW = b.getWidth() / (float) numSteps;
            int activeStep = currentStepFn ? currentStepFn() : -1;

            for (int i = 0; i < numSteps; ++i)
            {
                auto* v = apvts.getRawParameterValue (prefix + juce::String (i));
                float val01 = v != nullptr ? juce::jlimit (0.0f, 1.0f, v->load() / 100.0f) : 1.0f;

                auto cell = juce::Rectangle<float> (b.getX() + i * cellW, b.getY(), cellW, b.getHeight()).reduced (1.5f);
                g.setColour (i == activeStep ? Palette::panelHi.brighter (0.15f) : Palette::panel);
                g.fillRoundedRectangle (cell, 2.0f);

                auto bar = cell.withY (cell.getBottom() - cell.getHeight() * val01).withHeight (cell.getHeight() * val01);
                g.setColour (i == activeStep ? accent.brighter (0.3f) : accent.withAlpha (0.8f));
                g.fillRoundedRectangle (bar, 2.0f);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override { setFromMouse (e); }
        void mouseDrag (const juce::MouseEvent& e) override { setFromMouse (e); }

    private:
        void setFromMouse (const juce::MouseEvent& e)
        {
            float cellW = (float) getWidth() / (float) numSteps;
            int idx = juce::jlimit (0, numSteps - 1, (int) (e.position.x / cellW));
            float val01 = juce::jlimit (0.0f, 1.0f, 1.0f - (float) e.position.y / (float) getHeight());

            if (auto* p = apvts.getParameter (prefix + juce::String (idx)))
                p->setValueNotifyingHost (p->convertTo0to1 (val01 * 100.0f));
            repaint();
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::String prefix;
        juce::Colour accent;
        int numSteps = 8;
        std::function<int()> currentStepFn;
    };

    //==============================================================================
    // Click-to-cycle / drag-to-paint grid for the stutter/repeat/tape-stop tab.
    class StepActionGrid : public juce::Component
    {
    public:
        StepActionGrid (juce::AudioProcessorValueTreeState& s, juce::String pfx)
            : apvts (s), prefix (std::move (pfx)) {}

        void setNumSteps (int n) { numSteps = juce::jlimit (1, StutterEngine::maxSteps, n); repaint(); }
        void setCurrentStepProvider (std::function<int()> fn) { currentStepFn = std::move (fn); }

        static juce::Colour colourForAction (int action)
        {
            switch (action)
            {
                case 0: return Palette::stroke; // Off
                case 1: case 2: case 3: case 4: return Palette::teal;   // repeats
                case 5: return Palette::purple;   // reverse
                case 6: case 7: return Palette::orange; // tape stop/up
                case 8: case 9: return Palette::pink;   // pitch up/down
                case 10: return Palette::red;     // gate
                default: return Palette::stroke;
            }
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            float cellW = b.getWidth() / (float) numSteps;
            int activeStep = currentStepFn ? currentStepFn() : -1;
            auto choices = stutterActionChoices();

            for (int i = 0; i < numSteps; ++i)
            {
                auto* v = apvts.getRawParameterValue (prefix + juce::String (i));
                int action = v != nullptr ? (int) v->load() : 0;

                auto cell = juce::Rectangle<float> (b.getX() + i * cellW, b.getY(), cellW, b.getHeight()).reduced (1.5f);
                g.setColour (i == activeStep ? Palette::panelHi.brighter (0.2f) : Palette::bg1);
                g.fillRoundedRectangle (cell, 3.0f);

                if (action != 0)
                {
                    auto inner = cell.reduced (cell.getWidth() * 0.12f, cell.getHeight() * 0.12f);
                    g.setColour (colourForAction (action));
                    g.fillRoundedRectangle (inner, 2.0f);
                }
                g.setColour (Palette::stroke);
                g.drawRoundedRectangle (cell, 3.0f, 1.0f);

                if (cellW > 30.0f && action != 0 && action < choices.size())
                {
                    g.setColour (Palette::bg0);
                    g.setFont (juce::Font (juce::FontOptions (juce::jmin (10.0f, cell.getHeight() * 0.22f))));
                    g.drawFittedText (choices[action], cell.toNearestInt().reduced (2), juce::Justification::centred, 2);
                }
            }
        }

        void mouseDown (const juce::MouseEvent& e) override { cycleAt (e, true); }
        void mouseDrag (const juce::MouseEvent& e) override { cycleAt (e, false); }

    private:
        void cycleAt (const juce::MouseEvent& e, bool isNewClick)
        {
            float cellW = (float) getWidth() / (float) numSteps;
            int idx = juce::jlimit (0, numSteps - 1, (int) (e.position.x / cellW));
            if (! isNewClick && idx == lastPaintedIdx) return;
            lastPaintedIdx = idx;

            auto choices = stutterActionChoices();
            if (auto* p = apvts.getParameter (prefix + juce::String (idx)))
            {
                int current = (int) (p->getValue() * (choices.size() - 1) + 0.5f);
                int next = isNewClick ? (current + 1) % choices.size() : paintAction;
                if (isNewClick) paintAction = next;
                p->setValueNotifyingHost (p->convertTo0to1 ((float) next));
            }
            repaint();
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::String prefix;
        int numSteps = 16;
        int lastPaintedIdx = -1;
        int paintAction = 0;
        std::function<int()> currentStepFn;
    };
}
