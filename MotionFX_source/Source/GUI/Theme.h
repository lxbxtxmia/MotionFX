#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <BinaryData.h>
#include <vector>

namespace mfx
{
    struct ThemeDefinition
    {
        juce::String id;
        juce::String name;
        bool light = false;

        juce::Colour background;
        juce::Colour surface;
        juce::Colour panel;
        juce::Colour panelRaised;
        juce::Colour border;
        juce::Colour text;
        juce::Colour secondaryText;

        juce::Colour teal;
        juce::Colour purple;
        juce::Colour pink;
        juce::Colour orange;
        juce::Colour yellow;
        juce::Colour red;
        juce::Colour blue;
        juce::Colour green;
    };

    namespace Palette
    {
        inline juce::Colour bg0      { 0xff11131a };
        inline juce::Colour bg1      { 0xff181b24 };
        inline juce::Colour panel    { 0xff202430 };
        inline juce::Colour panelHi  { 0xff2a3040 };
        inline juce::Colour stroke   { 0xff41495d };
        inline juce::Colour text     { 0xfff3f5f8 };
        inline juce::Colour textDim  { 0xffaeb6c5 };
        inline juce::Colour teal     { 0xff36e0c8 };
        inline juce::Colour purple   { 0xffb06bff };
        inline juce::Colour pink     { 0xffff5fa8 };
        inline juce::Colour orange   { 0xffffa94d };
        inline juce::Colour yellow   { 0xfff5e15c };
        inline juce::Colour red      { 0xffff5a5a };
        inline juce::Colour blue     { 0xff72a7ff };
        inline juce::Colour green    { 0xff63d471 };
        inline bool isLight = false;

        inline juce::Colour effectColour (int effectIndex)
        {
            switch (effectIndex)
            {
                case 0: return orange;
                case 1: return teal;
                case 2: return yellow;
                case 3: return purple;
                case 4: return pink;
                case 5: return blue;
                case 6: return green;
                default: return teal;
            }
        }

        inline void apply (const ThemeDefinition& theme, bool highContrast)
        {
            isLight = theme.light;
            bg0 = theme.background;
            bg1 = theme.surface;
            panel = theme.panel;
            panelHi = theme.panelRaised;
            stroke = theme.border;
            text = theme.text;
            textDim = theme.secondaryText;
            teal = theme.teal;
            purple = theme.purple;
            pink = theme.pink;
            orange = theme.orange;
            yellow = theme.yellow;
            red = theme.red;
            blue = theme.blue;
            green = theme.green;

            if (highContrast)
            {
                if (isLight)
                {
                    bg0 = juce::Colour (0xfff7f8fa);
                    bg1 = juce::Colour (0xffffffff);
                    panel = juce::Colour (0xffeef1f5);
                    panelHi = juce::Colour (0xffe0e5ec);
                    stroke = juce::Colour (0xff3b4350);
                    text = juce::Colour (0xff05070a);
                    textDim = juce::Colour (0xff343b46);
                }
                else
                {
                    bg0 = juce::Colour (0xff080a0f);
                    bg1 = juce::Colour (0xff10131a);
                    panel = juce::Colour (0xff171c26);
                    panelHi = juce::Colour (0xff242c3a);
                    stroke = juce::Colour (0xffaeb8c9);
                    text = juce::Colour (0xffffffff);
                    textDim = juce::Colour (0xffd4dae4);
                }

                auto increaseAccent = [] (juce::Colour colour)
                {
                    return colour.withMultipliedSaturation (1.12f)
                                 .withMultipliedBrightness (1.08f);
                };

                teal = increaseAccent (teal);
                purple = increaseAccent (purple);
                pink = increaseAccent (pink);
                orange = increaseAccent (orange);
                yellow = increaseAccent (yellow);
                red = increaseAccent (red);
                blue = increaseAccent (blue);
                green = increaseAccent (green);
            }
        }
    }

    class UiPreferences
    {
    public:
        static UiPreferences& instance()
        {
            static UiPreferences settings;
            return settings;
        }

        const juce::String& getThemeId() const noexcept { return themeId; }
        bool isHighContrast() const noexcept { return highContrast; }
        bool isReducedMotion() const noexcept { return reducedMotion; }
        bool hasEnhancedControls() const noexcept { return enhancedControls; }
        bool hasLargerText() const noexcept { return largerText; }

        float getTextScale() const noexcept
        {
            return largerText ? 1.24f : 1.10f;
        }

        void setThemeId (juce::String newThemeId)
        {
            themeId = std::move (newThemeId);
            applyTheme();
            save();
        }

        void setHighContrast (bool enabled)
        {
            highContrast = enabled;
            applyTheme();
            save();
        }

        void setReducedMotion (bool enabled)
        {
            reducedMotion = enabled;
            save();
        }

        void setEnhancedControls (bool enabled)
        {
            enhancedControls = enabled;
            save();
        }

        void setLargerText (bool enabled)
        {
            largerText = enabled;
            save();
        }

        juce::File getThemeFolder() const
        {
            auto folder = juce::File::getSpecialLocation (
                              juce::File::userDocumentsDirectory)
                              .getChildFile ("MotionFX")
                              .getChildFile ("Themes");
            folder.createDirectory();
            return folder;
        }

        std::vector<ThemeDefinition> getAvailableThemes() const
        {
            std::vector<ThemeDefinition> themes {
                makeDarkTheme(),
                makeLightTheme()
            };

            const auto folder = getThemeFolder();

            for (const auto& file : folder.findChildFiles (
                     juce::File::findFiles, false, "*.mfxtheme"))
            {
                ThemeDefinition parsed;
                if (readThemeFile (file, parsed))
                    themes.push_back (parsed);
            }

            return themes;
        }

        ThemeDefinition getCurrentTheme() const
        {
            const auto themes = getAvailableThemes();

            for (const auto& theme : themes)
                if (theme.id == themeId)
                    return theme;

            return makeDarkTheme();
        }

        void applyTheme() const
        {
            Palette::apply (getCurrentTheme(), highContrast);
        }

        void createExampleThemeIfMissing() const
        {
            const auto example = getThemeFolder().getChildFile (
                "Example Midnight Blue.mfxtheme");

            if (example.existsAsFile())
                return;

            example.replaceWithText (R"JSON({
  "id": "custom.midnight-blue",
  "name": "Midnight Blue",
  "light": false,
  "background": "#0C111A",
  "surface": "#111A28",
  "panel": "#182438",
  "panelRaised": "#22324A",
  "border": "#52647F",
  "text": "#F4F7FB",
  "secondaryText": "#BAC5D4",
  "teal": "#42E4D0",
  "purple": "#B77AFF",
  "pink": "#FF69AE",
  "orange": "#FFAE5C",
  "yellow": "#F4DF66",
  "red": "#FF6262",
  "blue": "#78AFFF",
  "green": "#69D77A"
})JSON");
        }

    private:
        UiPreferences()
        {
            load();
            createExampleThemeIfMissing();
            applyTheme();
        }

        static ThemeDefinition makeDarkTheme()
        {
            return {
                "builtin.dark",
                "MotionFX Dark",
                false,
                juce::Colour (0xff11131a),
                juce::Colour (0xff181b24),
                juce::Colour (0xff202430),
                juce::Colour (0xff2a3040),
                juce::Colour (0xff41495d),
                juce::Colour (0xfff3f5f8),
                juce::Colour (0xffaeb6c5),
                juce::Colour (0xff36e0c8),
                juce::Colour (0xffb06bff),
                juce::Colour (0xffff5fa8),
                juce::Colour (0xffffa94d),
                juce::Colour (0xfff5e15c),
                juce::Colour (0xffff5a5a),
                juce::Colour (0xff72a7ff),
                juce::Colour (0xff63d471)
            };
        }

        static ThemeDefinition makeLightTheme()
        {
            return {
                "builtin.light",
                "MotionFX Light",
                true,
                juce::Colour (0xffeef1f5),
                juce::Colour (0xfff8f9fb),
                juce::Colour (0xffffffff),
                juce::Colour (0xffe4e8ee),
                juce::Colour (0xffa2acba),
                juce::Colour (0xff111722),
                juce::Colour (0xff4e5968),
                juce::Colour (0xff008f81),
                juce::Colour (0xff7540b8),
                juce::Colour (0xffc83273),
                juce::Colour (0xffb55b00),
                juce::Colour (0xff8a7600),
                juce::Colour (0xffc83838),
                juce::Colour (0xff356fca),
                juce::Colour (0xff2b8a3e)
            };
        }

        static juce::Colour readColour (const juce::DynamicObject& object,
                                        const juce::Identifier& key,
                                        juce::Colour fallback)
        {
            auto value = object.getProperty (key).toString().trim();

            if (value.startsWithChar ('#'))
                value = value.substring (1);

            if (value.length() == 6)
                value = "ff" + value;

            return value.length() == 8
                ? juce::Colour ((juce::uint32) value.getHexValue64())
                : fallback;
        }

        static bool readThemeFile (const juce::File& file,
                                   ThemeDefinition& destination)
        {
            const auto parsed = juce::JSON::parse (file.loadFileAsString());
            auto* object = parsed.getDynamicObject();

            if (object == nullptr)
                return false;

            const auto fallback = makeDarkTheme();
            destination.id = object->getProperty ("id").toString().trim();
            destination.name = object->getProperty ("name").toString().trim();

            if (destination.id.isEmpty())
                destination.id = "custom."
                               + file.getFileNameWithoutExtension()
                                     .toLowerCase()
                                     .replaceCharacter (' ', '-');

            if (destination.name.isEmpty())
                destination.name = file.getFileNameWithoutExtension();

            destination.light = (bool) object->getProperty ("light");
            destination.background = readColour (*object, "background", fallback.background);
            destination.surface = readColour (*object, "surface", fallback.surface);
            destination.panel = readColour (*object, "panel", fallback.panel);
            destination.panelRaised = readColour (*object, "panelRaised", fallback.panelRaised);
            destination.border = readColour (*object, "border", fallback.border);
            destination.text = readColour (*object, "text", fallback.text);
            destination.secondaryText = readColour (*object, "secondaryText", fallback.secondaryText);
            destination.teal = readColour (*object, "teal", fallback.teal);
            destination.purple = readColour (*object, "purple", fallback.purple);
            destination.pink = readColour (*object, "pink", fallback.pink);
            destination.orange = readColour (*object, "orange", fallback.orange);
            destination.yellow = readColour (*object, "yellow", fallback.yellow);
            destination.red = readColour (*object, "red", fallback.red);
            destination.blue = readColour (*object, "blue", fallback.blue);
            destination.green = readColour (*object, "green", fallback.green);
            return true;
        }

        juce::File getSettingsFile() const
        {
            auto folder = juce::File::getSpecialLocation (
                              juce::File::userApplicationDataDirectory)
                              .getChildFile ("MotionFX");
            folder.createDirectory();
            return folder.getChildFile ("ui_settings.json");
        }

        void load()
        {
            const auto file = getSettingsFile();
            if (! file.existsAsFile())
                return;

            const auto parsed = juce::JSON::parse (file.loadFileAsString());
            auto* object = parsed.getDynamicObject();

            if (object == nullptr)
                return;

            auto loadedTheme = object->getProperty ("theme").toString().trim();
            if (loadedTheme.isNotEmpty())
                themeId = loadedTheme;

            highContrast = (bool) object->getProperty ("highContrast");
            reducedMotion = (bool) object->getProperty ("reducedMotion");
            enhancedControls = (bool) object->getProperty ("enhancedControls");
            largerText = (bool) object->getProperty ("largerText");
        }

        void save() const
        {
            auto object = std::make_unique<juce::DynamicObject>();
            object->setProperty ("theme", themeId);
            object->setProperty ("highContrast", highContrast);
            object->setProperty ("reducedMotion", reducedMotion);
            object->setProperty ("enhancedControls", enhancedControls);
            object->setProperty ("largerText", largerText);

            const juce::var value (object.release());
            getSettingsFile().replaceWithText (
                juce::JSON::toString (value, true));
        }

        juce::String themeId { "builtin.dark" };
        bool highContrast = false;
        bool reducedMotion = false;
        bool enhancedControls = true;
        bool largerText = false;
    };

    class FontBank
    {
    public:
        static juce::Typeface::Ptr regularTypeface()
        {
            static auto typeface = loadTypeface ("Regular");
            return typeface;
        }

        static juce::Typeface::Ptr boldTypeface()
        {
            static auto typeface = loadTypeface ("Bold");
            return typeface;
        }

        static juce::Font font (float height, bool bold = false)
        {
            const float scaledHeight = height
                * UiPreferences::instance().getTextScale();
            auto typeface = bold ? boldTypeface() : regularTypeface();

            if (typeface != nullptr)
            {
                return juce::Font (
                    juce::FontOptions (typeface)
                        .withHeight (scaledHeight));
            }

            return juce::Font (
                juce::FontOptions (
                    scaledHeight,
                    bold ? juce::Font::bold : juce::Font::plain));
        }

    private:
        static juce::Typeface::Ptr loadTypeface (
            const juce::String& styleToken)
        {
            for (int index = 0;
                 index < BinaryData::namedResourceListSize;
                 ++index)
            {
                const juce::String resourceName (
                    BinaryData::namedResourceList[index]);

                if (! resourceName.containsIgnoreCase (
                        "AtkinsonHyperlegibleNext")
                    || ! resourceName.containsIgnoreCase (styleToken)
                    || ! resourceName.containsIgnoreCase ("ttf"))
                {
                    continue;
                }

                int size = 0;
                const auto* data = BinaryData::getNamedResource (
                    BinaryData::namedResourceList[index], size);

                if (data != nullptr && size > 0)
                    return juce::Typeface::createSystemTypefaceFor (
                        data, (size_t) size);
            }

            return {};
        }
    };
}
