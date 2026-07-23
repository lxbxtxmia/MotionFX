#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

class GuiTestApp : public juce::JUCEApplicationBase
{
public:
    const juce::String getApplicationName() override { return "MotionFXGuiTest"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted (const juce::String&) override {}
    void suspended() override {}
    void resumed() override {}
    void shutdown() override {}
    void systemRequestedQuit() override { juce::JUCEApplicationBase::quit(); }
    void unhandledException (const std::exception*, const juce::String&, int) override {}

    void initialise (const juce::String&) override
    {
        runTest();
        juce::JUCEApplicationBase::quit();
    }

    void runTest()
    {
        const auto oldTheme =
            mfx::UiPreferences::instance().getThemeId();
        const bool oldContrast =
            mfx::UiPreferences::instance().isHighContrast();
        const bool oldReducedMotion =
            mfx::UiPreferences::instance().isReducedMotion();
        const bool oldEnhanced =
            mfx::UiPreferences::instance().hasEnhancedControls();
        const bool oldLargeText =
            mfx::UiPreferences::instance().hasLargerText();

        struct ThemeCase
        {
            juce::String id;
            bool highContrast;
            juce::String filename;
        };

        const std::vector<ThemeCase> themeCases {
            { "builtin.dark", false, "dark" },
            { "builtin.light", false, "light" },
            { "builtin.dark", true, "dark_high_contrast" },
            { "builtin.light", true, "light_high_contrast" }
        };

        const std::vector<int> scales {
            25, 50, 75, 100, 150, 200, 300
        };

        auto snapshotDirectory =
            juce::File::getSpecialLocation (
                juce::File::tempDirectory)
                .getChildFile ("MotionFX")
                .getChildFile ("gui_snapshots");
        snapshotDirectory.createDirectory();

        for (const auto& themeCase : themeCases)
        {
            mfx::UiPreferences::instance().setThemeId (
                themeCase.id);
            mfx::UiPreferences::instance().setHighContrast (
                themeCase.highContrast);

            auto processor =
                std::make_unique<MotionFXAudioProcessor>();
            processor->prepareToPlay (48000.0, 512);

            auto editor =
                std::make_unique<
                    MotionFXAudioProcessorEditor> (*processor);

            juce::Component holder;
            holder.setBounds (0, 0, 3400, 2300);
            holder.setVisible (true);
            holder.addAndMakeVisible (*editor);
            editor->setTopLeftPosition (0, 0);

            for (const int scale : scales)
            {
                editor->setSize (
                    (int) (1080 * (scale / 100.0f)),
                    (int) (720 * (scale / 100.0f)));

                const auto bounds =
                    editor->getLocalBounds();

                if (bounds.getWidth() <= 0
                    || bounds.getHeight() <= 0)
                {
                    std::cout
                        << "  [FAIL] invalid editor bounds"
                        << std::endl;
                    continue;
                }

                juce::Image image (
                    juce::Image::ARGB,
                    bounds.getWidth(),
                    bounds.getHeight(),
                    true);

                {
                    juce::Graphics graphics (image);
                    editor->paintEntireComponent (
                        graphics,
                        true);
                }

                const auto outputFile =
                    snapshotDirectory.getChildFile (
                        themeCase.filename
                        + "_scale_"
                        + juce::String (scale)
                        + "pct.png");

                juce::PNGImageFormat png;
                juce::FileOutputStream stream (
                    outputFile);

                if (stream.openedOk())
                {
                    stream.setPosition (0);
                    stream.truncate();
                    png.writeImageToStream (
                        image,
                        stream);
                }
                else
                {
                    std::cout
                        << "  [FAIL] could not write "
                        << outputFile.getFullPathName()
                        << std::endl;
                }
            }
        }

        // Larger Text regression pass: branding and the fixed-width
        // Gain Match control must stay renderable without truncation.
        mfx::UiPreferences::instance().setThemeId ("builtin.light");
        mfx::UiPreferences::instance().setHighContrast (false);
        mfx::UiPreferences::instance().setLargerText (true);

        {
            auto processor =
                std::make_unique<MotionFXAudioProcessor>();
            processor->prepareToPlay (48000.0, 512);
            auto editor =
                std::make_unique<MotionFXAudioProcessorEditor> (*processor);
            editor->setSize (1080, 720);

            juce::Image image (
                juce::Image::ARGB,
                1080,
                720,
                true);
            juce::Graphics graphics (image);
            editor->paintEntireComponent (graphics, true);

            const auto outputFile = snapshotDirectory.getChildFile (
                "light_larger_text_100pct.png");
            juce::PNGImageFormat png;
            juce::FileOutputStream stream (outputFile);
            if (stream.openedOk())
            {
                stream.setPosition (0);
                stream.truncate();
                png.writeImageToStream (image, stream);
            }
        }

        mfx::UiPreferences::instance().setThemeId (
            oldTheme);
        mfx::UiPreferences::instance().setHighContrast (
            oldContrast);
        mfx::UiPreferences::instance().setReducedMotion (
            oldReducedMotion);
        mfx::UiPreferences::instance().setEnhancedControls (
            oldEnhanced);
        mfx::UiPreferences::instance().setLargerText (
            oldLargeText);

        std::cout
            << "GUI theme/resize test complete, no crashes."
            << std::endl;
    }

    int getExitCode() { return 0; }
};

START_JUCE_APPLICATION (GuiTestApp)
