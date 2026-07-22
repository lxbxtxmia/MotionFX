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
                    : Palette::textDim.withAlpha (0.45f));

            juce::Path path;

            switch (icon)
            {
                case HeaderIcon::Previous:
                    path.startNewSubPath (
                        bounds.getRight() - 2.0f,
                        bounds.getY());
                    path.lineTo (
                        bounds.getX() + 2.0f,
                        bounds.getCentreY());
                    path.lineTo (
                        bounds.getRight() - 2.0f,
                        bounds.getBottom());
                    break;

                case HeaderIcon::Next:
                    path.startNewSubPath (
                        bounds.getX() + 2.0f,
                        bounds.getY());
                    path.lineTo (
                        bounds.getRight() - 2.0f,
                        bounds.getCentreY());
                    path.lineTo (
                        bounds.getX() + 2.0f,
                        bounds.getBottom());
                    break;

                case HeaderIcon::Undo:
                case HeaderIcon::Redo:
                {
                    const bool redo = icon == HeaderIcon::Redo;
                    const float direction = redo ? 1.0f : -1.0f;
                    const float startX = redo
                        ? bounds.getX() + 2.0f
                        : bounds.getRight() - 2.0f;
                    const float endX = redo
                        ? bounds.getRight() - 2.0f
                        : bounds.getX() + 2.0f;

                    path.startNewSubPath (
                        startX,
                        bounds.getBottom() - 2.0f);
                    path.cubicTo (
                        startX,
                        bounds.getY() + 2.0f,
                        endX - direction * bounds.getWidth() * 0.22f,
                        bounds.getY() + 2.0f,
                        endX,
                        bounds.getCentreY());

                    juce::Path arrow;
                    arrow.startNewSubPath (
                        endX - direction * 5.0f,
                        bounds.getCentreY() - 5.0f);
                    arrow.lineTo (endX, bounds.getCentreY());
                    arrow.lineTo (
                        endX - direction * 5.0f,
                        bounds.getCentreY() + 5.0f);

                    graphics.strokePath (
                        path,
                        juce::PathStrokeType (
                            2.0f,
                            juce::PathStrokeType::curved,
                            juce::PathStrokeType::rounded));
                    graphics.strokePath (
                        arrow,
                        juce::PathStrokeType (
                            2.0f,
                            juce::PathStrokeType::curved,
                            juce::PathStrokeType::rounded));
                    return;
                }

                case HeaderIcon::Save:
                {
                    auto disk = bounds.reduced (1.0f);
                    graphics.drawRoundedRectangle (
                        disk,
                        2.0f,
                        1.7f);
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
                            : Palette::textDim.withAlpha (0.45f));
                    graphics.drawRoundedRectangle (
                        disk.reduced (
                            disk.getWidth() * 0.20f,
                            disk.getHeight() * 0.56f)
                            .translated (
                                0.0f,
                                disk.getHeight() * 0.25f),
                        1.5f,
                        1.4f);
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
                        1.7f);
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
                    2.2f,
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
                "Green: below -6 dBFS, yellow: -6 to 0 dBFS, "
                "red: above 0 dBFS before the safety ceiling.");
            startTimerHz (30);
        }

        void paint (juce::Graphics& graphics) override
        {
            auto bounds = getLocalBounds().toFloat();

            graphics.setColour (Palette::bg1);
            graphics.fillRoundedRectangle (bounds, 5.0f);
            graphics.setColour (Palette::stroke);
            graphics.drawRoundedRectangle (
                bounds.reduced (0.5f),
                5.0f,
                1.0f);

            bounds = bounds.reduced (6.0f, 5.0f);
            auto labelArea = bounds.removeFromBottom (12.0f);
            const float gap = 4.0f;
            const float barWidth =
                (bounds.getWidth() - gap) * 0.5f;

            const auto leftBar = bounds.removeFromLeft (barWidth);
            bounds.removeFromLeft (gap);
            const auto rightBar = bounds;

            drawBar (graphics, leftBar, displayedLeftDb);
            drawBar (graphics, rightBar, displayedRightDb);

            graphics.setFont (FontBank::font (8.5f, true));
            graphics.setColour (Palette::textDim);
            auto leftLabel = labelArea.removeFromLeft (barWidth);
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

        static float dbToY (float db,
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

        static void fillDbRange (
            juce::Graphics& graphics,
            juce::Rectangle<float> area,
            float visibleDb,
            float rangeBottomDb,
            float rangeTopDb,
            juce::Colour colour)
        {
            if (visibleDb <= rangeBottomDb)
                return;

            const float clippedTop = juce::jmin (
                visibleDb,
                rangeTopDb);
            const float topY = dbToY (clippedTop, area);
            const float bottomY = dbToY (
                rangeBottomDb,
                area);

            graphics.setColour (colour);
            graphics.fillRoundedRectangle (
                area.withY (topY)
                    .withHeight (
                        juce::jmax (0.0f, bottomY - topY)),
                1.5f);
        }

        static void drawBar (
            juce::Graphics& graphics,
            juce::Rectangle<float> area,
            float db)
        {
            graphics.setColour (Palette::panel);
            graphics.fillRoundedRectangle (area, 2.0f);

            fillDbRange (
                graphics,
                area,
                db,
                -60.0f,
                -6.0f,
                juce::Colour (0xff52d273));
            fillDbRange (
                graphics,
                area,
                db,
                -6.0f,
                0.0f,
                juce::Colour (0xffffd166));
            fillDbRange (
                graphics,
                area,
                db,
                0.0f,
                6.0f,
                juce::Colour (0xffff5a5a));

            graphics.setColour (
                Palette::stroke.withAlpha (0.78f));
            graphics.drawRoundedRectangle (
                area,
                2.0f,
                1.0f);

            graphics.setColour (
                Palette::text.withAlpha (0.36f));
            graphics.drawHorizontalLine (
                (int) dbToY (-6.0f, area),
                area.getX(),
                area.getRight());
            graphics.drawHorizontalLine (
                (int) dbToY (0.0f, area),
                area.getX(),
                area.getRight());
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
            return juce::jmax (-60.0f, current - fallPerTick);
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
