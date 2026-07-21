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
        auto proc = std::make_unique<MotionFXAudioProcessor>();
        proc->prepareToPlay (48000.0, 512);

        auto editor = std::make_unique<MotionFXAudioProcessorEditor> (*proc);

        struct Step { int pct; };
        std::vector<Step> steps = { {25}, {50}, {75}, {100}, {150}, {200}, {300} };

        juce::Component holder;
        holder.setBounds (0, 0, 3400, 2300);
        holder.setVisible (true);
        holder.addAndMakeVisible (*editor);
        editor->setTopLeftPosition (0, 0);

        for (auto& s : steps)
        {
            editor->setSize ((int) (1080 * (s.pct / 100.0f)), (int) (720 * (s.pct / 100.0f)));

            auto bounds = editor->getLocalBounds();
            std::cout << "Scale " << s.pct << "%: editor size = " << bounds.getWidth() << "x" << bounds.getHeight() << std::endl;

            juce::Image img (juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
            {
                juce::Graphics g (img);
                editor->paintEntireComponent (g, true);
            }

            auto outFile = juce::File ("/home/claude/gui_snapshots/scale_" + juce::String (s.pct) + "pct.png");
            outFile.getParentDirectory().createDirectory();
            juce::PNGImageFormat png;
            juce::FileOutputStream stream (outFile);
            if (stream.openedOk())
            {
                stream.setPosition (0);
                stream.truncate();
                png.writeImageToStream (img, stream);
                std::cout << "  wrote " << outFile.getFullPathName() << std::endl;
            }
            else
            {
                std::cout << "  [FAIL] could not write " << outFile.getFullPathName() << std::endl;
            }
        }

        editor.reset();
        proc.reset();
        std::cout << "GUI resize test complete, no crashes." << std::endl;
    }

    int getExitCode() { return 0; }
};

START_JUCE_APPLICATION (GuiTestApp)
