#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/EffectChain.h"
#include <functional>
#include <vector>
#include <algorithm>

namespace mfx
{
    class PresetManager
    {
    public:
        struct UserPreset
        {
            juce::String displayName;
            juce::String relativePath;
            juce::File file;
        };

        using OrderGetter = std::function<std::array<EffectId, numEffects>()>;
        using OrderSetter = std::function<void (std::array<EffectId, numEffects>)>;

        void attach (juce::AudioProcessorValueTreeState* s, OrderGetter g, OrderSetter setr)
        {
            apvts = s; orderGetter = std::move (g); orderSetter = std::move (setr);
            if (defaultStateXml.isEmpty()) defaultStateXml = buildStateXmlString();
            refreshUserPresetList();
        }

        static juce::File getDefaultPresetDirectory();
        juce::File getPresetDirectory() const;
        void setPresetDirectory (const juce::File& dir);

        void loadInitPreset();
        int getNumFactoryPresets() const;
        juce::String getFactoryPresetName (int index) const;
        void loadFactoryPreset (int index);

        void refreshUserPresetList();
        const std::vector<UserPreset>& getUserPresets() const noexcept { return userPresets; }
        bool saveUserPreset (const juce::String& name, const juce::String& relativeFolder = {});
        bool loadUserPresetByPath (const juce::String& relativePath);
        bool deleteUserPresetByPath (const juce::String& relativePath);
        bool createPresetFolder (const juce::String& relativeFolder);

        juce::StringArray getAllPresetNames() const;
        void loadByCombinedIndex (int index);
        int getCurrentIndex() const noexcept { return currentIndex; }
        juce::String getCurrentName() const noexcept { return currentName; }
        void next();
        void previous();

        juce::String getFullStateXml() const { return buildStateXmlString(); }
        void restoreFullStateXml (const juce::String& xml) { applyStateXmlString (xml); }

    private:
        juce::String buildStateXmlString() const;
        void applyStateXmlString (const juce::String& xml);
        juce::File resolveRelativePresetPath (const juce::String& relativePath) const;

        juce::AudioProcessorValueTreeState* apvts = nullptr;
        OrderGetter orderGetter;
        OrderSetter orderSetter;
        juce::File presetDirOverride;
        std::vector<UserPreset> userPresets;
        int currentIndex = 0;
        juce::String currentName = "Init";
        juce::String defaultStateXml;
    };
}
