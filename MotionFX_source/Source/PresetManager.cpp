#include "PresetManager.h"
#include "FactoryPresets.h"
#include <array>

namespace mfx
{
    namespace
    {
        std::array<int, 3> parseVersion (juce::String version)
        {
            version = version.upToFirstOccurrenceOf ("-", false, false)
                             .upToFirstOccurrenceOf ("+", false, false);

            juce::StringArray parts;
            parts.addTokens (version, ".", juce::String());
            parts.trim();
            parts.removeEmptyStrings();

            std::array<int, 3> result { 0, 0, 0 };
            for (int index = 0; index < juce::jmin (3, parts.size()); ++index)
                result[(size_t) index] = parts[index].getIntValue();
            return result;
        }

        bool isNewerVersion (const juce::String& candidate,
                             const juce::String& current)
        {
            const auto candidateParts = parseVersion (candidate);
            const auto currentParts = parseVersion (current);

            for (int index = 0; index < 3; ++index)
            {
                if (candidateParts[(size_t) index] != currentParts[(size_t) index])
                    return candidateParts[(size_t) index] > currentParts[(size_t) index];
            }

            return false;
        }

        void setParam (juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& id,
                       float value)
        {
            if (auto* parameter = apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }
    }

    juce::File PresetManager::getDefaultPresetDirectory()
    {
        auto directory = juce::File::getSpecialLocation (
                             juce::File::userDocumentsDirectory)
                             .getChildFile ("MotionFX")
                             .getChildFile ("Presets");

        if (! directory.exists())
            directory.createDirectory();

        return directory;
    }

    juce::String PresetManager::getDefaultAuthor()
    {
        auto author = juce::SystemStats::getFullUserName().trim();
        if (author.isEmpty())
            author = juce::SystemStats::getLogonName().trim();
        return author.isNotEmpty() ? author : juce::String ("Unknown");
    }

    juce::File PresetManager::getPresetDirectory() const
    {
        return presetDirOverride != juce::File()
            ? presetDirOverride
            : getDefaultPresetDirectory();
    }

    void PresetManager::setPresetDirectory (const juce::File& directory)
    {
        presetDirOverride = directory;

        if (! presetDirOverride.exists())
            presetDirOverride.createDirectory();

        refreshUserPresetList();
    }

    void PresetManager::loadInitPreset()
    {
        lastError.clear();

        if (defaultStateXml.isEmpty() && apvts != nullptr)
            defaultStateXml = buildStateXmlString (false);

        applyStateXmlString (defaultStateXml);

        if (orderSetter)
            orderSetter ({ EffectId::Drive, EffectId::Retro, EffectId::Filter,
                           EffectId::Pan, EffectId::Width, EffectId::Volume,
                           EffectId::Space });

        currentName = "Init";
        currentIndex = 0;
        markCurrentStateClean();
    }

    int PresetManager::getNumFactoryPresets() const
    {
        return (int) getFactoryPresets().size();
    }

    juce::String PresetManager::getFactoryPresetName (int index) const
    {
        const auto presets = getFactoryPresets();
        return index >= 0 && index < (int) presets.size()
            ? presets[(size_t) index].name
            : juce::String {};
    }

    void PresetManager::loadFactoryPreset (int index)
    {
        lastError.clear();

        if (apvts == nullptr)
            return;

        const auto presets = getFactoryPresets();
        if (index < 0 || index >= (int) presets.size())
            return;

        loadInitPreset();

        for (const auto& overrideValue : presets[(size_t) index].overrides)
            setParam (*apvts, overrideValue.first, overrideValue.second);

        currentName = presets[(size_t) index].name;
        currentIndex = 1 + index;
        markCurrentStateClean();
    }

    juce::File PresetManager::resolveRelativePresetPath (
        const juce::String& relativePath) const
    {
        auto normalised = relativePath.replaceCharacter ('\\', '/')
                                      .trimCharactersAtStart ("/");

        if (normalised.contains (".."))
            return {};

        return getPresetDirectory().getChildFile (normalised);
    }

    void PresetManager::refreshUserPresetList()
    {
        userPresets.clear();
        const auto root = getPresetDirectory();

        if (! root.exists())
            return;

        for (const auto& file : root.findChildFiles (
                 juce::File::findFiles, true, "*.mfxpreset"))
        {
            const auto relative = file.getRelativePathFrom (root)
                                      .replaceCharacter ('\\', '/');
            userPresets.push_back ({
                file.getFileNameWithoutExtension(),
                relative,
                file
            });
        }

        std::sort (userPresets.begin(), userPresets.end(),
                   [] (const UserPreset& first, const UserPreset& second)
        {
            return first.relativePath.compareIgnoreCase (second.relativePath) < 0;
        });

        if (defaultStateXml.isEmpty() && apvts != nullptr)
            defaultStateXml = buildStateXmlString (false);
    }

    juce::String PresetManager::buildStateXmlString (
        bool includePresetMetadata,
        bool includeFileMetadata,
        const juce::String& requestedAuthor) const
    {
        if (apvts == nullptr)
            return {};

        auto state = apvts->copyState();
        auto xml = state.createXml();

        if (orderGetter)
        {
            const auto order = orderGetter();
            auto* orderXml = xml->createNewChildElement ("ORDER");

            for (int index = 0; index < (int) order.size(); ++index)
                orderXml->setAttribute (
                    "slot" + juce::String (index),
                    (int) order[(size_t) index]);
        }

        if (includePresetMetadata)
        {
            auto* metadata = xml->createNewChildElement ("PRESET_META");
            metadata->setAttribute ("name", currentName);
            metadata->setAttribute ("index", currentIndex);
            metadata->setAttribute ("hasCleanHash", hasCleanStateHash ? 1 : 0);
            metadata->setAttribute ("cleanHash", juce::String (cleanStateHash));
        }

        if (includeFileMetadata)
        {
            auto author = requestedAuthor.trim();
            if (author.isEmpty())
                author = getDefaultAuthor();

            auto* metadata = xml->createNewChildElement ("FILE_META");
            metadata->setAttribute ("schemaVersion", 2);
            metadata->setAttribute ("creator", author);
            metadata->setAttribute ("createdWithVersion", getCurrentSoftwareVersion());
            metadata->setAttribute (
                "createdUtc",
                juce::Time::getCurrentTime().toISO8601 (true));
        }

        return xml->toString();
    }

    bool PresetManager::applyStateXmlString (
        const juce::String& xmlString,
        bool restorePresetMetadata)
    {
        lastError.clear();

        if (apvts == nullptr || xmlString.isEmpty())
        {
            lastError = "The preset contains no readable state.";
            return false;
        }

        auto xml = juce::XmlDocument::parse (xmlString);
        if (xml == nullptr)
        {
            lastError = "The preset file is invalid or damaged.";
            return false;
        }

        if (auto* fileMetadata = xml->getChildByName ("FILE_META"))
        {
            const auto presetVersion = fileMetadata->getStringAttribute (
                "createdWithVersion");

            if (presetVersion.isNotEmpty()
                && isNewerVersion (presetVersion, getCurrentSoftwareVersion()))
            {
                lastError = "This preset was created with MotionFX "
                          + presetVersion
                          + ", but this plug-in is MotionFX "
                          + getCurrentSoftwareVersion()
                          + ". Update MotionFX before loading this preset.";
                return false;
            }

            xml->removeChildElement (fileMetadata, true);
        }

        bool restoredMetadata = false;

        if (auto* metadata = xml->getChildByName ("PRESET_META"))
        {
            if (restorePresetMetadata)
            {
                currentName = metadata->getStringAttribute (
                    "name", "Restored Session");
                currentIndex = metadata->getIntAttribute ("index", 0);
                hasCleanStateHash = metadata->getBoolAttribute (
                    "hasCleanHash", false);
                cleanStateHash = metadata->getStringAttribute (
                    "cleanHash").getLargeIntValue();
                restoredMetadata = true;
            }

            xml->removeChildElement (metadata, true);
        }

        if (auto* orderXml = xml->getChildByName ("ORDER"))
        {
            if (orderSetter)
            {
                std::array<EffectId, numEffects> order;

                for (int index = 0; index < numEffects; ++index)
                {
                    order[(size_t) index] = (EffectId) orderXml->getIntAttribute (
                        "slot" + juce::String (index), index);
                }

                orderSetter (order);
            }

            xml->removeChildElement (orderXml, true);
        }

        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid())
        {
            lastError = "The preset state could not be reconstructed.";
            return false;
        }

        apvts->replaceState (tree);

        if (restorePresetMetadata && (! restoredMetadata || ! hasCleanStateHash))
        {
            if (! restoredMetadata)
            {
                currentName = "Restored Session";
                currentIndex = 0;
            }

            markCurrentStateClean();
        }

        return true;
    }

    juce::int64 PresetManager::computeStateHash() const
    {
        return buildStateXmlString (false).hashCode64();
    }

    void PresetManager::markCurrentStateClean()
    {
        cleanStateHash = computeStateHash();
        hasCleanStateHash = true;
    }

    bool PresetManager::isCurrentPresetModified() const
    {
        return hasCleanStateHash && computeStateHash() != cleanStateHash;
    }

    juce::String PresetManager::getDisplayName() const
    {
        return currentName + (isCurrentPresetModified() ? "*" : "");
    }

    bool PresetManager::createPresetFolder (
        const juce::String& relativeFolder)
    {
        const auto folder = resolveRelativePresetPath (relativeFolder);
        return folder != juce::File()
            && (folder.isDirectory() || folder.createDirectory());
    }

    bool PresetManager::saveUserPreset (
        const juce::String& name,
        const juce::String& relativeFolder,
        const juce::String& author)
    {
        lastError.clear();

        if (apvts == nullptr || name.trim().isEmpty())
        {
            lastError = "Enter a valid preset name.";
            return false;
        }

        const auto directory = relativeFolder.isEmpty()
            ? getPresetDirectory()
            : resolveRelativePresetPath (relativeFolder);

        if (directory == juce::File()
            || (! directory.exists() && ! directory.createDirectory()))
        {
            lastError = "The selected preset folder could not be created.";
            return false;
        }

        const auto file = directory.getChildFile (
            juce::File::createLegalFileName (name.trim()) + ".mfxpreset");

        const bool succeeded = file.replaceWithText (
            buildStateXmlString (false, true, author));

        if (! succeeded)
        {
            lastError = "The preset file could not be written.";
            return false;
        }

        currentName = name.trim();
        refreshUserPresetList();

        const auto relative = file.getRelativePathFrom (getPresetDirectory())
                                  .replaceCharacter ('\\', '/');

        for (int index = 0; index < (int) userPresets.size(); ++index)
        {
            if (userPresets[(size_t) index].relativePath == relative)
                currentIndex = 1 + getNumFactoryPresets() + index;
        }

        markCurrentStateClean();
        return true;
    }

    bool PresetManager::loadUserPresetByPath (
        const juce::String& relativePath)
    {
        lastError.clear();

        const auto file = resolveRelativePresetPath (relativePath);
        if (file == juce::File() || ! file.existsAsFile())
        {
            lastError = "The preset file could not be found.";
            return false;
        }

        if (! applyStateXmlString (file.loadFileAsString()))
            return false;

        currentName = file.getFileNameWithoutExtension();

        for (int index = 0; index < (int) userPresets.size(); ++index)
        {
            if (userPresets[(size_t) index].relativePath == relativePath)
                currentIndex = 1 + getNumFactoryPresets() + index;
        }

        markCurrentStateClean();
        return true;
    }

    bool PresetManager::deleteUserPresetByPath (
        const juce::String& relativePath)
    {
        const auto file = resolveRelativePresetPath (relativePath);
        const bool succeeded = file != juce::File()
                            && file.existsAsFile()
                            && file.deleteFile();

        if (succeeded)
            refreshUserPresetList();

        return succeeded;
    }

    juce::StringArray PresetManager::getAllPresetNames() const
    {
        juce::StringArray all { "Init" };

        for (const auto& factoryPreset : getFactoryPresets())
            all.add (factoryPreset.name);

        for (const auto& userPreset : userPresets)
            all.add (userPreset.displayName);

        return all;
    }

    bool PresetManager::loadByCombinedIndex (int index)
    {
        const int factoryCount = getNumFactoryPresets();

        if (index <= 0)
        {
            loadInitPreset();
            return true;
        }

        if (index <= factoryCount)
        {
            loadFactoryPreset (index - 1);
            return true;
        }

        const int userIndex = index - 1 - factoryCount;
        if (userIndex >= 0 && userIndex < (int) userPresets.size())
            return loadUserPresetByPath (
                userPresets[(size_t) userIndex].relativePath);

        lastError = "The selected preset no longer exists.";
        return false;
    }

    bool PresetManager::next()
    {
        const auto all = getAllPresetNames();
        return ! all.isEmpty()
            && loadByCombinedIndex ((currentIndex + 1) % all.size());
    }

    bool PresetManager::previous()
    {
        const auto all = getAllPresetNames();
        return ! all.isEmpty()
            && loadByCombinedIndex (
                (currentIndex - 1 + all.size()) % all.size());
    }
}
