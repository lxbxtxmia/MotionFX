#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <vector>

namespace mfx
{
    class StateHistory final : private juce::AudioProcessorValueTreeState::Listener,
                               private juce::Timer
    {
    public:
        struct HistoryItem
        {
            int nodeIndex = -1;
            juce::String text;
            bool current = false;
            int depth = 0;
        };

        using SnapshotGetter = std::function<juce::String()>;
        using SnapshotRestorer = std::function<void (const juce::String&)>;

        StateHistory (juce::AudioProcessorValueTreeState& state,
                      juce::AudioProcessor& processor,
                      SnapshotGetter getter,
                      SnapshotRestorer restorer)
            : apvts (state),
              snapshotGetter (std::move (getter)),
              snapshotRestorer (std::move (restorer)),
              journalFile (getJournalFile())
        {
            for (auto* parameter : processor.getParameters())
            {
                if (auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                {
                    const auto parameterId = identified->paramID;
                    parameterIds.addIfNotAlreadyThere (parameterId);
                    apvts.addParameterListener (parameterId, this);
                }
            }

            loadPreviousJournal();

            Node rootNode;
            rootNode.label = "Session start";
            rootNode.snapshot = snapshotGetter != nullptr ? snapshotGetter() : juce::String {};
            nodes.push_back (std::move (rootNode));
            currentNode = 0;

            writeJournal (false);
            ensureTimerRunning();
        }

        ~StateHistory() override
        {
            stopTimer();

            for (const auto& parameterId : parameterIds)
                apvts.removeParameterListener (parameterId, this);

            writeJournal (true);
        }

        void ensureTimerRunning()
        {
            if (! isTimerRunning()
                && juce::MessageManager::getInstanceWithoutCreating() != nullptr)
            {
                startTimerHz (10);
            }
        }

        bool canUndo() const noexcept
        {
            return currentNode >= 0
                && currentNode < (int) nodes.size()
                && nodes[(size_t) currentNode].parent >= 0;
        }

        bool canRedo() const noexcept
        {
            return findRedoChild (currentNode) >= 0;
        }

        void undo()
        {
            if (! canUndo())
                return;

            const int child = currentNode;
            const int parent = nodes[(size_t) child].parent;
            nodes[(size_t) parent].preferredChild = child;
            currentNode = parent;
            restoreCurrentNode();
        }

        void redo()
        {
            const int child = findRedoChild (currentNode);
            if (child < 0)
                return;

            nodes[(size_t) currentNode].preferredChild = child;
            currentNode = child;
            restoreCurrentNode();
        }

        void jumpToNode (int nodeIndex)
        {
            if (nodeIndex < 0 || nodeIndex >= (int) nodes.size() || nodeIndex == currentNode)
                return;

            currentNode = nodeIndex;
            restoreCurrentNode();
        }

        void notifyExternalChange (const juce::String& label = "Edit")
        {
            pendingLabel = label;
            dirty.store (true, std::memory_order_relaxed);
            lastChangeMilliseconds.store (juce::Time::getMillisecondCounter(),
                                          std::memory_order_relaxed);
        }

        void flushPendingNow (const juce::String& fallbackLabel = "Parameter edit")
        {
            if (! dirty.exchange (false, std::memory_order_relaxed))
                return;

            const auto label = pendingLabel.isNotEmpty() ? pendingLabel : fallbackLabel;
            pendingLabel.clear();
            captureSnapshot (label);
        }

        std::vector<HistoryItem> getHistoryItems() const
        {
            std::vector<HistoryItem> result;
            result.reserve (nodes.size());

            for (int index = 0; index < (int) nodes.size(); ++index)
            {
                int depth = 0;
                int parent = nodes[(size_t) index].parent;
                int guard = 0;

                while (parent >= 0 && parent < (int) nodes.size()
                       && guard++ < (int) nodes.size())
                {
                    ++depth;
                    parent = nodes[(size_t) parent].parent;
                }

                result.push_back ({
                    index,
                    nodes[(size_t) index].label,
                    index == currentNode,
                    depth
                });
            }

            return result;
        }

        int getNumHistoryPoints() const noexcept
        {
            return (int) nodes.size();
        }

        int getBranchCountAt (int nodeIndex) const noexcept
        {
            if (nodeIndex < 0 || nodeIndex >= (int) nodes.size())
                return 0;

            int count = 0;
            for (const auto& node : nodes)
                if (node.parent == nodeIndex)
                    ++count;
            return count;
        }

        bool hasCrashRecovery() const noexcept
        {
            return ! recoveredNodes.empty()
                && recoveredCurrentNode >= 0
                && recoveredCurrentNode < (int) recoveredNodes.size();
        }

        bool restoreCrashRecovery()
        {
            if (! hasCrashRecovery())
                return false;

            nodes = recoveredNodes;
            currentNode = recoveredCurrentNode;
            recoveredNodes.clear();
            recoveredCurrentNode = -1;
            restoreCurrentNode();
            return true;
        }

        void dismissCrashRecovery()
        {
            recoveredNodes.clear();
            recoveredCurrentNode = -1;
        }

    private:
        struct Node
        {
            int parent = -1;
            int preferredChild = -1;
            juce::String label;
            juce::String snapshot;
        };

        void parameterChanged (const juce::String&, float) override
        {
            if (restoring.load (std::memory_order_relaxed))
                return;

            dirty.store (true, std::memory_order_relaxed);
            lastChangeMilliseconds.store (juce::Time::getMillisecondCounter(),
                                          std::memory_order_relaxed);
        }

        void timerCallback() override
        {
            if (! dirty.load (std::memory_order_relaxed))
                return;

            const auto now = juce::Time::getMillisecondCounter();
            const auto last = lastChangeMilliseconds.load (std::memory_order_relaxed);

            if ((juce::uint32) (now - last) >= 300u)
                flushPendingNow();
        }

        void captureSnapshot (const juce::String& label)
        {
            if (snapshotGetter == nullptr)
                return;

            const auto snapshot = snapshotGetter();
            if (snapshot.isEmpty())
                return;

            if (currentNode >= 0 && currentNode < (int) nodes.size()
                && nodes[(size_t) currentNode].snapshot == snapshot)
                return;

            Node node;
            node.parent = currentNode;
            node.label = label + "  "
                       + juce::Time::getCurrentTime().toString (false, true, true, true);
            node.snapshot = snapshot;

            const int newIndex = (int) nodes.size();
            nodes.push_back (std::move (node));

            if (currentNode >= 0 && currentNode < newIndex)
                nodes[(size_t) currentNode].preferredChild = newIndex;

            currentNode = newIndex;
            writeJournal (false);
        }

        void restoreCurrentNode()
        {
            if (currentNode < 0 || currentNode >= (int) nodes.size()
                || snapshotRestorer == nullptr)
                return;

            restoring.store (true, std::memory_order_relaxed);
            snapshotRestorer (nodes[(size_t) currentNode].snapshot);
            restoring.store (false, std::memory_order_relaxed);

            dirty.store (false, std::memory_order_relaxed);
            pendingLabel.clear();
            writeJournal (false);
        }

        int findRedoChild (int nodeIndex) const noexcept
        {
            if (nodeIndex < 0 || nodeIndex >= (int) nodes.size())
                return -1;

            const int preferred = nodes[(size_t) nodeIndex].preferredChild;
            if (preferred >= 0 && preferred < (int) nodes.size()
                && nodes[(size_t) preferred].parent == nodeIndex)
                return preferred;

            for (int index = (int) nodes.size() - 1; index >= 0; --index)
                if (nodes[(size_t) index].parent == nodeIndex)
                    return index;

            return -1;
        }

        static juce::File getJournalFile()
        {
            auto directory = juce::File::getSpecialLocation (
                                 juce::File::userApplicationDataDirectory)
                                 .getChildFile ("MotionFX")
                                 .getChildFile ("Recovery");
            directory.createDirectory();
            return directory.getChildFile ("latest_session_history.xml");
        }

        static bool readJournal (const juce::XmlElement& root,
                                 std::vector<Node>& destination,
                                 int& destinationCurrent)
        {
            destination.clear();
            destinationCurrent = root.getIntAttribute ("currentNode", -1);

            for (auto* element = root.getFirstChildElement();
                 element != nullptr;
                 element = element->getNextElement())
            {
                if (! element->hasTagName ("NODE"))
                    continue;

                auto* stateElement = element->getChildByName ("STATE");
                if (stateElement == nullptr)
                    continue;

                Node node;
                node.parent = element->getIntAttribute ("parent", -1);
                node.preferredChild = element->getIntAttribute ("preferredChild", -1);
                node.label = element->getStringAttribute ("label", "Recovered edit");
                node.snapshot = stateElement->getAllSubText();

                if (node.snapshot.isNotEmpty())
                    destination.push_back (std::move (node));
            }

            if (destination.empty())
            {
                destinationCurrent = -1;
                return false;
            }

            destinationCurrent = juce::jlimit (
                0, (int) destination.size() - 1, destinationCurrent);

            for (auto& node : destination)
            {
                if (node.parent >= (int) destination.size())
                    node.parent = -1;
                if (node.preferredChild >= (int) destination.size())
                    node.preferredChild = -1;
            }

            return true;
        }

        void loadPreviousJournal()
        {
            if (! journalFile.existsAsFile())
                return;

            auto xml = juce::XmlDocument::parse (journalFile);
            if (xml == nullptr || ! xml->hasTagName ("MOTIONFX_HISTORY"))
                return;

            if (xml->getBoolAttribute ("cleanShutdown", true))
                return;

            readJournal (*xml, recoveredNodes, recoveredCurrentNode);
        }

        void writeJournal (bool cleanShutdown) const
        {
            if (journalFile == juce::File())
                return;

            auto root = std::make_unique<juce::XmlElement> ("MOTIONFX_HISTORY");
            root->setAttribute ("formatVersion", 1);
            root->setAttribute ("pluginVersion", "0.9.1");
            root->setAttribute ("cleanShutdown", cleanShutdown);
            root->setAttribute ("currentNode", currentNode);

            for (int index = 0; index < (int) nodes.size(); ++index)
            {
                const auto& node = nodes[(size_t) index];
                auto* element = root->createNewChildElement ("NODE");
                element->setAttribute ("index", index);
                element->setAttribute ("parent", node.parent);
                element->setAttribute ("preferredChild", node.preferredChild);
                element->setAttribute ("label", node.label);
                element->createNewChildElement ("STATE")->addTextElement (node.snapshot);
            }

            journalFile.getParentDirectory().createDirectory();
            juce::TemporaryFile temporaryFile (journalFile);

            if (temporaryFile.getFile().replaceWithText (root->toString()))
                temporaryFile.overwriteTargetFileWithTemporary();
        }

        juce::AudioProcessorValueTreeState& apvts;
        SnapshotGetter snapshotGetter;
        SnapshotRestorer snapshotRestorer;
        juce::StringArray parameterIds;
        juce::File journalFile;

        std::vector<Node> nodes;
        int currentNode = -1;

        std::vector<Node> recoveredNodes;
        int recoveredCurrentNode = -1;

        std::atomic<bool> dirty { false };
        std::atomic<bool> restoring { false };
        std::atomic<juce::uint32> lastChangeMilliseconds { 0 };
        juce::String pendingLabel;
    };
}
