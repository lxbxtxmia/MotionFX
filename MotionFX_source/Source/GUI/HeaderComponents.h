#pragma once
#include "LookAndFeel.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

namespace mfx
{
    enum class HeaderIcon
    {
        Previous,
        Next,
        Undo,
        Redo,
        Save,
        Settings
    };

    class IconButton final : public juce::Button
    {
    public:
        explicit IconButton (HeaderIcon iconToDraw)
            : juce::Button (""), icon (iconToDraw)
        {
            setWantsKeyboardFocus (true);
        }

        void paintButton (juce::Graphics& graphics,
                          bool highlighted,
                          bool down) override
        {
            getLookAndFeel().drawButtonBackground (
                graphics,
                *this,
                Palette::panel,
                highlighted,
                down);

            auto bounds = getLocalBounds()
                              .toFloat()
                              .reduced (juce::jmax (
                                  6.0f,
                                  getWidth() * 0.22f));

            graphics.setColour (
                isEnabled()
                    ? Palette::text
                    : Palette::textDim.withAlpha (0.48f));

            juce::Path path;

            switch (icon)
            {
                case HeaderIcon::Previous:
                {
                    juce::Path arrow;
                    arrow.addTriangle (
                        bounds.getX() + 1.0f,
                        bounds.getCentreY(),
                        bounds.getRight() - 2.0f,
                        bounds.getY() + 1.5f,
                        bounds.getRight() - 2.0f,
                        bounds.getBottom() - 1.5f);
                    graphics.fillPath (arrow);
                    return;
                }

                case HeaderIcon::Next:
                {
                    juce::Path arrow;
                    arrow.addTriangle (
                        bounds.getRight() - 1.0f,
                        bounds.getCentreY(),
                        bounds.getX() + 2.0f,
                        bounds.getY() + 1.5f,
                        bounds.getX() + 2.0f,
                        bounds.getBottom() - 1.5f);
                    graphics.fillPath (arrow);
                    return;
                }

                case HeaderIcon::Undo:
                case HeaderIcon::Redo:
                {
                    const bool redo = icon == HeaderIcon::Redo;
                    const float direction = redo ? 1.0f : -1.0f;
                    const float tipX = redo
                        ? bounds.getRight() - 1.0f
                        : bounds.getX() + 1.0f;
                    const float arrowY = bounds.getCentreY() - 4.0f;
                    const float baseX = tipX - direction * 7.0f;
                    const float farX = redo
                        ? bounds.getX() + 3.0f
                        : bounds.getRight() - 3.0f;
                    const float bottomY = bounds.getBottom() - 2.0f;

                    juce::Path shaft;
                    shaft.startNewSubPath (
                        baseX - direction * 1.5f,
                        arrowY);
                    shaft.lineTo (farX, arrowY);
                    shaft.quadraticTo (
                        farX - direction * 3.0f,
                        arrowY,
                        farX - direction * 3.0f,
                        arrowY + 3.0f);
                    shaft.lineTo (
                        farX - direction * 3.0f,
                        bottomY);

                    graphics.strokePath (
                        shaft,
                        juce::PathStrokeType (
                            2.2f,
                            juce::PathStrokeType::curved,
                            juce::PathStrokeType::rounded));

                    juce::Path arrowHead;
                    arrowHead.addTriangle (
                        tipX,
                        arrowY,
                        baseX,
                        arrowY - 5.0f,
                        baseX,
                        arrowY + 5.0f);
                    graphics.fillPath (arrowHead);
                    return;
                }

                case HeaderIcon::Save:
                {
                    auto disk = bounds.reduced (1.0f);
                    graphics.drawRoundedRectangle (
                        disk,
                        2.0f,
                        1.8f);
                    graphics.fillRect (
                        disk.getX() + disk.getWidth() * 0.22f,
                        disk.getY(),
                        disk.getWidth() * 0.50f,
                        disk.getHeight() * 0.34f);
                    graphics.setColour (Palette::panel);
                    graphics.fillRect (
                        disk.getX() + disk.getWidth() * 0.30f,
                        disk.getY() + 1.5f,
                        disk.getWidth() * 0.28f,
                        disk.getHeight() * 0.22f);
                    graphics.setColour (
                        isEnabled()
                            ? Palette::text
                            : Palette::textDim.withAlpha (0.48f));
                    graphics.drawRoundedRectangle (
                        disk.reduced (
                            disk.getWidth() * 0.20f,
                            disk.getHeight() * 0.56f)
                            .translated (
                                0.0f,
                                disk.getHeight() * 0.25f),
                        1.5f,
                        1.5f);
                    return;
                }

                case HeaderIcon::Settings:
                {
                    const auto centre = bounds.getCentre();
                    const float outer = bounds.getWidth() * 0.42f;
                    const float inner = bounds.getWidth() * 0.15f;

                    for (int index = 0; index < 8; ++index)
                    {
                        const float angle =
                            juce::MathConstants<float>::twoPi
                            * (float) index / 8.0f;
                        const auto start = centre
                            + juce::Point<float> (
                                std::cos (angle),
                                std::sin (angle))
                              * (outer * 0.68f);
                        const auto end = centre
                            + juce::Point<float> (
                                std::cos (angle),
                                std::sin (angle))
                              * outer;

                        graphics.drawLine (
                            juce::Line<float> (start, end),
                            2.0f);
                    }

                    graphics.drawEllipse (
                        centre.x - outer * 0.66f,
                        centre.y - outer * 0.66f,
                        outer * 1.32f,
                        outer * 1.32f,
                        1.8f);
                    graphics.fillEllipse (
                        centre.x - inner,
                        centre.y - inner,
                        inner * 2.0f,
                        inner * 2.0f);
                    return;
                }
            }

            graphics.strokePath (
                path,
                juce::PathStrokeType (
                    2.3f,
                    juce::PathStrokeType::curved,
                    juce::PathStrokeType::rounded));
        }

    private:
        HeaderIcon icon;
    };

    class StereoOutputMeter final : public juce::Component,
                                    public juce::SettableTooltipClient,
                                    private juce::Timer
    {
    public:
        StereoOutputMeter (std::atomic<float>& leftSource,
                           std::atomic<float>& rightSource,
                           std::atomic<juce::uint64>& epochSource)
            : left (leftSource),
              right (rightSource),
              signalEpoch (epochSource)
        {
            lastEpoch = signalEpoch.load (
                std::memory_order_relaxed);
            setTooltip (
                "Post-effect stereo peak meter. "
                "Green below -6 dBFS, yellow from -6 to 0 dBFS, "
                "red above 0 dBFS before the safety ceiling.");
            startTimerHz (30);
        }

        void paint (juce::Graphics& graphics) override
        {
            auto bounds = getLocalBounds().toFloat();
            auto labelArea = bounds.removeFromBottom (13.0f);
            bounds = bounds.reduced (2.0f, 1.0f);

            const float gap = 5.0f;
            const float barWidth =
                (bounds.getWidth() - gap) * 0.5f;

            const auto leftBar =
                bounds.removeFromLeft (barWidth);
            bounds.removeFromLeft (gap);
            const auto rightBar = bounds;

            drawBar (graphics, leftBar, displayedLeftDb);
            drawBar (graphics, rightBar, displayedRightDb);

            graphics.setFont (FontBank::font (9.5f, true));
            graphics.setColour (Palette::textDim);

            auto leftLabel =
                labelArea.removeFromLeft (barWidth);
            labelArea.removeFromLeft (gap);

            graphics.drawText (
                "L",
                leftLabel.toNearestInt(),
                juce::Justification::centred);
            graphics.drawText (
                "R",
                labelArea.toNearestInt(),
                juce::Justification::centred);
        }

    private:
        static float levelToDb (float level) noexcept
        {
            return juce::Decibels::gainToDecibels (
                juce::jmax (0.0f, level),
                -60.0f);
        }

        static float dbToY (
            float db,
            juce::Rectangle<float> area) noexcept
        {
            const float normalised = juce::jmap (
                juce::jlimit (-60.0f, 6.0f, db),
                -60.0f,
                6.0f,
                0.0f,
                1.0f);

            return area.getBottom()
                - normalised * area.getHeight();
        }

        static void drawBar (
            juce::Graphics& graphics,
            juce::Rectangle<float> area,
            float db)
        {
            graphics.setColour (
                Palette::panel.withAlpha (0.82f));
            graphics.fillRect (area);

            const float top = dbToY (db, area);
            auto fillArea = area.withY (top)
                                .withHeight (
                                    juce::jmax (
                                        0.0f,
                                        area.getBottom() - top));

            if (! fillArea.isEmpty())
            {
                juce::ColourGradient gradient (
                    juce::Colour (0xff55d679),
                    area.getCentreX(),
                    area.getBottom(),
                    juce::Colour (0xffff5a5a),
                    area.getCentreX(),
                    area.getY(),
                    false);

                const float yellowPosition =
                    (-6.0f + 60.0f) / 66.0f;
                const float zeroPosition =
                    (0.0f + 60.0f) / 66.0f;

                gradient.addColour (
                    yellowPosition,
                    juce::Colour (0xffffd166));
                gradient.addColour (
                    zeroPosition,
                    juce::Colour (0xffffb347));

                graphics.setGradientFill (gradient);
                graphics.fillRect (fillArea);
            }

            graphics.setColour (
                Palette::stroke.withAlpha (0.90f));
            graphics.drawRect (area, 1.0f);
        }

        static float smoothMeter (
            float current,
            float target) noexcept
        {
            if (target >= current)
                return target;

            const float fallPerTick =
                UiPreferences::instance().isReducedMotion()
                    ? 2.8f
                    : 1.2f;

            return juce::jmax (
                -60.0f,
                current - fallPerTick);
        }

        void timerCallback() override
        {
            const auto epoch = signalEpoch.load (
                std::memory_order_relaxed);

            if (epoch != lastEpoch)
            {
                lastEpoch = epoch;
                staleTicks = 0;
            }
            else
            {
                ++staleTicks;
            }

            const bool audioIsFresh = staleTicks <= 2;
            const float targetLeft = audioIsFresh
                ? levelToDb (
                    left.load (std::memory_order_relaxed))
                : -60.0f;
            const float targetRight = audioIsFresh
                ? levelToDb (
                    right.load (std::memory_order_relaxed))
                : -60.0f;

            displayedLeftDb = smoothMeter (
                displayedLeftDb,
                targetLeft);
            displayedRightDb = smoothMeter (
                displayedRightDb,
                targetRight);
            repaint();
        }

        std::atomic<float>& left;
        std::atomic<float>& right;
        std::atomic<juce::uint64>& signalEpoch;
        juce::uint64 lastEpoch = 0;
        int staleTicks = 0;
        float displayedLeftDb = -60.0f;
        float displayedRightDb = -60.0f;
    };
}
