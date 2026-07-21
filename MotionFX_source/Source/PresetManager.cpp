#include "PresetManager.h"
#include "FactoryPresets.h"

namespace mfx
{
    juce::File PresetManager::getDefaultPresetDirectory()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                       .getChildFile ("MotionFX").getChildFile ("Presets");
        if (! dir.exists()) dir.createDirectory();
        return dir;
    }

    juce::File PresetManager::getPresetDirectory() const
    {
        return presetDirOverride != juce::File() ? presetDirOverride : getDefaultPresetDirectory();
    }

    void PresetManager::setPresetDirectory (const juce::File& dir)
    {
        presetDirOverride = dir;
        if (! presetDirOverride.exists()) presetDirOverride.createDirectory();
        refreshUserPresetList();
    }

    void PresetManager::loadInitPreset()
    {
        if (defaultStateXml.isEmpty() && apvts != nullptr) defaultStateXml = buildStateXmlString (false);
        applyStateXmlString (defaultStateXml);
        if (orderSetter)
            orderSetter ({ EffectId::Drive, EffectId::Retro, EffectId::Pan,
                            EffectId::Width, EffectId::Volume, EffectId::Space });
        currentName = "Init";
        currentIndex = 0;
        markCurrentStateClean();
    }

    int PresetManager::getNumFactoryPresets() const { return (int) getFactoryPresets().size(); }

    juce::String PresetManager::getFactoryPresetName (int index) const
    {
        auto fps = getFactoryPresets();
        return (index >= 0 && index < (int) fps.size()) ? fps[(size_t) index].name : juce::String{};
    }

    static void setParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    void PresetManager::loadFactoryPreset (int index)
    {
        if (apvts == nullptr) return;
        auto fps = getFactoryPresets();
        if (index < 0 || index >= (int) fps.size()) return;
        loadInitPreset();
        for (auto& ov : fps[(size_t) index].overrides) setParam (*apvts, ov.first, ov.second);
        currentName = fps[(size_t) index].name;
        currentIndex = 1 + index;
        markCurrentStateClean();
    }

    juce::File PresetManager::resolveRelativePresetPath (const juce::String& relativePath) const
    {
        auto normalised = relativePath.replaceCharacter ('\\', '/').trimCharactersAtStart ("/");
        if (normalised.contains ("..")) return {};
        return getPresetDirectory().getChildFile (normalised);
    }

    void PresetManager::refreshUserPresetList()
    {
        userPresets.clear();
        auto root = getPresetDirectory();
        if (! root.exists()) return;
        for (const auto& f : root.findChildFiles (juce::File::findFiles, true, "*.mfxpreset"))
        {
            auto relative = f.getRelativePathFrom (root).replaceCharacter ('\\', '/');
            userPresets.push_back ({ f.getFileNameWithoutExtension(), relative, f });
        }
        std::sort (userPresets.begin(), userPresets.end(), [] (const UserPreset& a, const UserPreset& b)
        {
            return a.relativePath.compareIgnoreCase (b.relativePath) < 0;
        });
        if (defaultStateXml.isEmpty() && apvts != nullptr) defaultStateXml = buildStateXmlString (false);
    }

    juce::String PresetManager::buildStateXmlString (bool includePresetMetadata) const
    {
        if (apvts == nullptr) return {};
        auto state = apvts->copyState();
        auto xml = state.createXml();
        if (orderGetter)
        {
            auto order = orderGetter();
            auto* orderXml = xml->createNewChildElement ("ORDER");
            for (int i = 0; i < (int) order.size(); ++i)
                orderXml->setAttribute ("slot" + juce::String (i), (int) order[(size_t) i]);
        }

        if (includePresetMetadata)
        {
            auto* metadata = xml->createNewChildElement ("PRESET_META");
            metadata->setAttribute ("name", currentName);
            metadata->setAttribute ("index", currentIndex);
            metadata->setAttribute ("hasCleanHash", hasCleanStateHash ? 1 : 0);
            metadata->setAttribute ("cleanHash", juce::String (cleanStateHash));
        }

        return xml->toString();
    }

    void PresetManager::applyStateXmlString (const juce::String& xmlString, bool restorePresetMetadata)
    {
        if (apvts == nullptr || xmlString.isEmpty()) return;
        auto xml = juce::XmlDocument::parse (xmlString);
        if (xml == nullptr) return;

        bool restoredMetadata = false;
        if (auto* metadata = xml->getChildByName ("PRESET_META"))
        {
            if (restorePresetMetadata)
            {
                currentName = metadata->getStringAttribute ("name", "Restored Session");
                currentIndex = metadata->getIntAttribute ("index", 0);
                hasCleanStateHash = metadata->getBoolAttribute ("hasCleanHash", false);
                cleanStateHash = metadata->getStringAttribute ("cleanHash").getLargeIntValue();
                restoredMetadata = true;
            }

            xml->removeChildElement (metadata, true);
        }

        if (auto* orderXml = xml->getChildByName ("ORDER"))
        {
            if (orderSetter)
            {
                std::array<EffectId, numEffects> order;
                for (int i = 0; i < numEffects; ++i)
                    order[(size_t) i] = (EffectId) orderXml->getIntAttribute ("slot" + juce::String (i), i);
                orderSetter (order);
            }
            xml->removeChildElement (orderXml, true);
        }
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.isValid())
        {
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
        }
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

    bool PresetManager::createPresetFolder (const juce::String& relativeFolder)
    {
        auto folder = resolveRelativePresetPath (relativeFolder);
        return folder != juce::File() && (folder.isDirectory() || folder.createDirectory());
    }

    bool PresetManager::saveUserPreset (const juce::String& name, const juce::String& relativeFolder)
    {
        if (apvts == nullptr || name.trim().isEmpty()) return false;
        auto dir = relativeFolder.isEmpty() ? getPresetDirectory() : resolveRelativePresetPath (relativeFolder);
        if (dir == juce::File() || (! dir.exists() && ! dir.createDirectory())) return false;
        auto file = dir.getChildFile (juce::File::createLegalFileName (name.trim()) + ".mfxpreset");
        const bool ok = file.replaceWithText (buildStateXmlString (false));
        if (ok)
        {
            currentName = name.trim();
            refreshUserPresetList();
            const auto relative = file.getRelativePathFrom (getPresetDirectory()).replaceCharacter ('\\', '/');
            for (int i = 0; i < (int) userPresets.size(); ++i)
                if (userPresets[(size_t) i].relativePath == relative)
                    currentIndex = 1 + getNumFactoryPresets() + i;
            markCurrentStateClean();
        }
        return ok;
    }

    bool PresetManager::loadUserPresetByPath (const juce::String& relativePath)
    {
        auto file = resolveRelativePresetPath (relativePath);
        if (file == juce::File() || ! file.existsAsFile()) return false;
        applyStateXmlString (file.loadFileAsString());
        currentName = file.getFileNameWithoutExtension();
        for (int i = 0; i < (int) userPresets.size(); ++i)
            if (userPresets[(size_t) i].relativePath == relativePath)
                currentIndex = 1 + getNumFactoryPresets() + i;
        markCurrentStateClean();
        return true;
    }

    bool PresetManager::deleteUserPresetByPath (const juce::String& relativePath)
    {
        auto file = resolveRelativePresetPath (relativePath);
        const bool ok = file != juce::File() && file.existsAsFile() && file.deleteFile();
        if (ok) refreshUserPresetList();
        return ok;
    }

    juce::StringArray PresetManager::getAllPresetNames() const
    {
        juce::StringArray all { "Init" };
        for (auto& fp : getFactoryPresets()) all.add (fp.name);
        for (auto& up : userPresets) all.add (up.displayName);
        return all;
    }

    void PresetManager::loadByCombinedIndex (int index)
    {
        const int factoryCount = getNumFactoryPresets();
        if (index <= 0) { loadInitPreset(); return; }
        if (index <= factoryCount) { loadFactoryPreset (index - 1); return; }
        const int userIdx = index - 1 - factoryCount;
        if (userIdx >= 0 && userIdx < (int) userPresets.size())
            loadUserPresetByPath (userPresets[(size_t) userIdx].relativePath);
    }

    void PresetManager::next()
    {
        auto all = getAllPresetNames();
        if (! all.isEmpty()) loadByCombinedIndex ((currentIndex + 1) % all.size());
    }

    void PresetManager::previous()
    {
        auto all = getAllPresetNames();
        if (! all.isEmpty()) loadByCombinedIndex ((currentIndex - 1 + all.size()) % all.size());
    }
}
