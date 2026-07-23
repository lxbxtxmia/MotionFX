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

        void attach (juce::AudioProcessorValueTreeState* state,
                     OrderGetter getter,
                     OrderSetter setter)
        {
            apvts = state;
            orderGetter = std::move (getter);
            orderSetter = std::move (setter);

            if (defaultStateXml.isEmpty())
                defaultStateXml = buildStateXmlString (false);

            refreshUserPresetList();
            markCurrentStateClean();
        }

        static juce::File getDefaultPresetDirectory();
        static juce::String getCurrentSoftwareVersion() { return "0.10.0"; }
        static juce::String getDefaultAuthor();

        juce::File getPresetDirectory() const;
        void setPresetDirectory (const juce::File& directory);

        void loadInitPreset();
        int getNumFactoryPresets() const;
        juce::String getFactoryPresetName (int index) const;
        void loadFactoryPreset (int index);

        void refreshUserPresetList();
        const std::vector<UserPreset>& getUserPresets() const noexcept { return userPresets; }

        bool saveUserPreset (const juce::String& name,
                             const juce::String& relativeFolder = {},
                             const juce::String& author = {});
        bool loadUserPresetByPath (const juce::String& relativePath);
        bool deleteUserPresetByPath (const juce::String& relativePath);
        bool createPresetFolder (const juce::String& relativeFolder);

        juce::StringArray getAllPresetNames() const;
        bool loadByCombinedIndex (int index);
        int getCurrentIndex() const noexcept { return currentIndex; }
        juce::String getCurrentName() const noexcept { return currentName; }
        juce::String getDisplayName() const;
        bool isCurrentPresetModified() const;
        bool next();
        bool previous();

        juce::String takeLastError()
        {
            auto result = lastError;
            lastError.clear();
            return result;
        }

        juce::String getFullStateXml() const
        {
            return buildStateXmlString (true);
        }

        void restoreFullStateXml (const juce::String& xml)
        {
            applyStateXmlString (xml, true);
        }

    private:
        juce::String buildStateXmlString (bool includePresetMetadata,
                                          bool includeFileMetadata = false,
                                          const juce::String& author = {}) const;
        bool applyStateXmlString (const juce::String& xml,
                                  bool restorePresetMetadata = false);
        juce::int64 computeStateHash() const;
        void markCurrentStateClean();
        juce::File resolveRelativePresetPath (const juce::String& relativePath) const;

        juce::AudioProcessorValueTreeState* apvts = nullptr;
        OrderGetter orderGetter;
        OrderSetter orderSetter;
        juce::File presetDirOverride;
        std::vector<UserPreset> userPresets;
        int currentIndex = 0;
        juce::String currentName = "Init";
        juce::String defaultStateXml;
        juce::int64 cleanStateHash = 0;
        bool hasCleanStateHash = false;
        juce::String lastError;
    };
}
