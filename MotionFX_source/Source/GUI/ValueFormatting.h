#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>

namespace mfx
{
    class ModulationDisplayProvider
    {
    public:
        virtual ~ModulationDisplayProvider() = default;

        virtual bool getModulationDisplay (
            float& minimumNormalised,
            float& maximumNormalised,
            float& currentNormalised) const noexcept = 0;
    };

    namespace ValueFormatting
    {
        inline juce::String normaliseNumberText (juce::String text)
        {
            return text.trim().replaceCharacter (',', '.').toLowerCase();
        }

        inline juce::String fixed (double value, int decimals)
        {
            return juce::String (value, decimals);
        }

        inline juce::String compactDecimal (double value, int maximumDecimals)
        {
            auto result = juce::String (value, maximumDecimals);

            while (result.containsChar ('.') && result.endsWithChar ('0'))
                result = result.dropLastCharacters (1);

            if (result.endsWithChar ('.'))
                result = result.dropLastCharacters (1);

            return result;
        }

        inline double parseEngineeringValue (juce::String text)
        {
            text = normaliseNumberText (text);
            double multiplier = 1.0;

            if (text.contains ("mhz"))
                multiplier = 1000000.0;
            else if (text.contains ("khz") || text.endsWithChar ('k'))
                multiplier = 1000.0;

            return text.getDoubleValue() * multiplier;
        }

        inline juce::String frequencyHz (double hz, bool detailed)
        {
            hz = juce::jmax (0.0, hz);

            if (detailed)
                return fixed (hz, 2) + " Hz";

            if (hz >= 1000.0)
                return fixed (hz / 1000.0, 1) + " kHz";

            if (hz >= 100.0)
                return juce::String ((int) std::lround (hz)) + " Hz";

            if (hz >= 10.0)
            {
                const bool effectivelyInteger =
                    std::abs (hz - std::round (hz)) < 0.05;
                return compactDecimal (hz, effectivelyInteger ? 0 : 1) + " Hz";
            }

            if (hz >= 1.0)
                return compactDecimal (hz, 1) + " Hz";

            return compactDecimal (hz, 2) + " Hz";
        }

        inline juce::String percent (double value, bool detailed)
        {
            return detailed
                ? fixed (value, 2) + " %"
                : juce::String ((int) std::lround (value)) + " %";
        }

        inline juce::String decibels (double value)
        {
            return fixed (value, 2) + " dB";
        }

        inline juce::String milliseconds (double value, bool detailed)
        {
            if (detailed)
                return fixed (value, 2) + " ms";

            if (value >= 1000.0)
                return fixed (value / 1000.0, value < 10000.0 ? 2 : 1) + " s";

            if (value >= 100.0)
                return juce::String ((int) std::lround (value)) + " ms";

            return compactDecimal (value, value < 10.0 ? 1 : 0) + " ms";
        }

        inline double parseMilliseconds (juce::String text)
        {
            text = normaliseNumberText (text);
            const double value = text.getDoubleValue();

            if (text.contains (" ms"))
                return value;

            if (text.containsChar ('s'))
                return value * 1000.0;

            return value;
        }

        inline juce::String seconds (double value, bool detailed)
        {
            if (detailed)
                return fixed (value, 2) + " s";

            if (value < 1.0)
                return juce::String ((int) std::lround (value * 1000.0)) + " ms";

            return compactDecimal (value, value < 10.0 ? 2 : 1) + " s";
        }

        inline double parseSeconds (juce::String text)
        {
            text = normaliseNumberText (text);
            const double value = text.getDoubleValue();

            if (text.contains ("ms"))
                return value / 1000.0;

            return value;
        }

        inline juce::String pan (double value, bool detailed)
        {
            value = juce::jlimit (0.0, 100.0, value);

            if (detailed)
                return fixed (value, 2) + " %";

            const double offset = value - 50.0;

            if (std::abs (offset) < 0.5)
                return "C";

            const int amount = (int) std::lround (std::abs (offset));
            return juce::String (amount) + (offset < 0.0 ? "L" : "R");
        }

        inline double parsePan (juce::String text)
        {
            text = normaliseNumberText (text);

            if (text == "c" || text == "center" || text == "centre")
                return 50.0;

            const double amount = std::abs (text.getDoubleValue());

            if (text.containsChar ('l'))
                return juce::jlimit (0.0, 100.0, 50.0 - amount);

            if (text.containsChar ('r'))
                return juce::jlimit (0.0, 100.0, 50.0 + amount);

            return juce::jlimit (0.0, 100.0, text.getDoubleValue());
        }

        inline void modulationRange (float baseNormalised,
                                     float depthNormalised,
                                     float& minimumNormalised,
                                     float& maximumNormalised) noexcept
        {
            const float base = juce::jlimit (0.0f, 1.0f, baseNormalised);
            const float depth = juce::jlimit (0.0f, 1.0f, depthNormalised);

            minimumNormalised = base * (1.0f - depth);
            maximumNormalised = juce::jlimit (
                0.0f,
                1.0f,
                minimumNormalised + depth);
        }

        inline float contrastRatio (juce::Colour foreground,
                                    juce::Colour background) noexcept
        {
            const auto luminance = [] (juce::Colour colour)
            {
                const auto channel = [] (float value)
                {
                    value /= 255.0f;
                    return value <= 0.04045f
                        ? value / 12.92f
                        : std::pow ((value + 0.055f) / 1.055f, 2.4f);
                };

                return 0.2126f * channel ((float) colour.getRed())
                     + 0.7152f * channel ((float) colour.getGreen())
                     + 0.0722f * channel ((float) colour.getBlue());
            };

            const float first = luminance (foreground);
            const float second = luminance (background);
            const float lighter = juce::jmax (first, second);
            const float darker = juce::jmin (first, second);
            return (lighter + 0.05f) / (darker + 0.05f);
        }
    }
}
