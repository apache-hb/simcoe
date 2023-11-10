#include "engine/config/system.h"

#include "engine/core/panic.h"
#include "engine/core/strings.h"

#include "engine/log/service.h"

#include <ranges>

using namespace simcoe;
using namespace simcoe::config;


// TODO: make threadsafe
// auto getMutex = []() -> mt::Mutex& {
//     static mt::Mutex m{"config"};
//     return m;
// };

struct ConfigGroup final : IConfigEntry {
    ConfigGroup(std::string_view name, std::string_view description)
        : IConfigEntry({ .name = name, .description = description, .type = eConfigGroup, .flags = eDefault })
    {
        LOG_INFO("creating config group {}", name);
    }

    void addEntry(IConfigEntry *pEntry) {
        SM_ASSERT(pEntry);
        SM_ASSERTF(getEntry(std::string(pEntry->getName())) == nullptr, "entry with name {} already exists in {}", pEntry->getName(), getName());

        children.emplace(pEntry->getName(), pEntry);
    }

    IConfigEntry *getEntry(const std::string& name) {
        if (auto it = children.find(name); it != children.end()) {
            return it->second;
        }

        return nullptr;
    }

    bool isModified() const override {
        return true; // pretend we're always modified to make saving code simpler
    }

    bool readConfigValue(const INode *pNode) override {
        if (NodeMap map; pNode->get(map)) {
            for (auto& [name, pChild] : getChildren()) {
                if (auto it = map.find(name); it != map.end()) {
                    pChild->readConfigValue(it->second);
                }
            }

            return true;
        } else {
            LOG_WARN("failed to parse config group {}", getName());
            return false;
        }
    }

    const ConfigMap& getChildren() const override { return children; }

private:
    ConfigMap children;
};

static auto getRootGroup = []() -> ConfigGroup* {
    static ConfigGroup root("", "");
    return &root;
};

// add an entry to the config on a given path
// creating groups as needed, if path is empty then add to the root config
static void addToConfig(std::string_view path, IConfigEntry *pEntry) {
    SM_ASSERT(pEntry != nullptr);

    auto *pConfig = getRootGroup();
    if (path.empty()) {
        pConfig->addEntry(pEntry);
        return;
    }

    // path will be in the form of "group1/group2/group3"
    // we need to split it into groups and add them to the config
    // util::SplitView split(path, "/");
    for (auto word : util::SplitView(path, "/")) {
        auto segment = std::string(word);

        LOG_INFO("adding {} of {}", segment, path);

        // check if the group already exists
        if (auto *pExisting = pConfig->getEntry(segment); pExisting) {
            // check if the existing entry is a group
            if (pExisting->getType() != eConfigGroup) {
                SM_NEVER("entry {} is not a group (while adding {} to {})", segment, pEntry->getName(), path);
            }

            // move down the tree
            pConfig = static_cast<ConfigGroup*>(pExisting);
        } else {
            // create a new group
            auto *pNewGroup = new ConfigGroup(segment, "");
            pConfig->addEntry(pNewGroup);
            pConfig = pNewGroup;
        }
    }

    // add the entry to the last group
    pConfig->addEntry(pEntry);
}

IConfigEntry::IConfigEntry(std::string_view path, const ConfigEntryInfo& info)
    : entryInfo(info)
{
    SM_ASSERT(!info.name.empty());
    // descriptions and categories are optional

    // add ourselves to the config map
    addToConfig(path, this);
}

IConfigEntry::IConfigEntry(const ConfigEntryInfo& info)
    : entryInfo(info)
{ }

IConfigEntry *config::getConfig() {
    return getRootGroup();
}
