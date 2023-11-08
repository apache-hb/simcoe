#include "engine/config/system.h"

#include "engine/core/panic.h"
#include "engine/core/strings.h"

#include "engine/log/service.h"

#include <ranges>

using namespace simcoe;
using namespace simcoe::config;


// TODO: make threadsafe
// auto getMutex = []() -> std::mutex& {
//     static std::mutex m;
//     return m;
// };

struct ConfigGroup final : ConfigEntry {
    ConfigGroup(std::string name, std::string_view description)
        : ConfigEntry({ .name = name, .description = description, .type = eConfigGroup, .flags = eConfigDefault })
    {
        LOG_INFO("creating config group {}", name);
    }

    void addEntry(ConfigEntry *pEntry) {
        SM_ASSERT(pEntry);
        SM_ASSERTF(getEntry(std::string(pEntry->getName())) == nullptr, "entry with name {} already exists in {}", pEntry->getName(), getName());

        children.emplace(pEntry->getName(), pEntry);
    }

    ConfigEntry *getEntry(const std::string& name) {
        if (auto it = children.find(name); it != children.end()) {
            return it->second;
        }

        return nullptr;
    }

    bool isModified() const override {
        return true; // pretend we're always modified to make saving code simpler
    }

    void setCurrentValue(SM_UNUSED const void *pData) override {
        SM_NEVER("setCurrentValue used on group {}", getName());
    }

    bool parseValue(SM_UNUSED const INode *pNode) override {
        if (NodeMap map; pNode->get(map)) {
            for (auto& [name, pChild] : getChildren()) {
                if (auto it = map.find(name); it != map.end()) {
                    pChild->parseValue(it->second);
                }
            }

            return true;
        } else {
            LOG_WARN("failed to parse config group {}", getName());
            return false;
        }
    }

    void saveValue(SM_UNUSED void *pData, SM_UNUSED size_t size) const override {
        SM_NEVER("saveValue used on group {}", getName());
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
static void addToConfig(std::string_view path, ConfigEntry *pEntry) {
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

ConfigEntry::ConfigEntry(std::string_view path, const ConfigEntryInfo& info)
    : info(info)
{
    SM_ASSERT(!info.name.empty());
    // descriptions and categories are optional

    // add ourselves to the config map
    addToConfig(path, this);
}

ConfigEntry::ConfigEntry(const ConfigEntryInfo& info)
    : info(info)
{ }

ConfigEntry *config::getConfig() {
    return getRootGroup();
}
