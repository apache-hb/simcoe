#include "engine/config/service.h"

#include "engine/config/system.h"
#include "engine/config/source.h"

#include "engine/log/service.h"

using namespace simcoe;
using namespace simcoe::config;

bool ConfigService::createService() {
    fs::path cfg = fs::current_path() / "config.toml";
    config::ISource *pSource = config::newTomlSource();
    const config::INode *pRoot = pSource->load(cfg);
    if (pRoot == nullptr) {
        LOG_ERROR("failed to load config file {}", cfg.string());
        return false;
    }

    config::ConfigEntry *pConfig = config::getConfig();

    pConfig->parseValue(pRoot);

    return true;
}

void ConfigService::destroyService() {

}

bool ConfigService::loadConfig(const fs::path&) {
    return false;
}

static const INode *collapseConfig(ISource *pSource, ConfigEntry *pEntry, bool bModifiedOnly) {
    if (bModifiedOnly && !pEntry->isModified()) {
        return nullptr;
    }

    switch (pEntry->getType()) {
    case eConfigBool: {
        bool value = false;
        pEntry->saveValue(&value, sizeof(bool));
        return pSource->create(value);
    }
    case eConfigInt: {
        int64_t value = 0;
        pEntry->saveValue(&value, sizeof(int64_t));
        return pSource->create(value);
    }
    case eConfigFloat: {
        float value = 0;
        pEntry->saveValue(&value, sizeof(float));
        return pSource->create(value);
    }
    case eConfigEnum:
    case eConfigFlags:
    case eConfigString: {
        std::string value;
        pEntry->saveValue(&value, sizeof(std::string));
        return pSource->create(value);
    }
    case eConfigGroup: {
        NodeMap map;
        for (auto& [name, pChild] : pEntry->getChildren()) {
            if (const INode *pNode = collapseConfig(pSource, pChild, bModifiedOnly); pNode != nullptr) {
                map[name] = pNode;
            }
        }

        return pSource->create(map);
    }

    default:
        SM_NEVER("collapseConfig not implemented for type {}", pEntry->getType());
    }
}

bool ConfigService::saveConfig(const fs::path& path, bool bModifiedOnly) {
    config::ISource *pSource = config::newTomlSource();
    const INode *pRoot = collapseConfig(pSource, config::getConfig(), bModifiedOnly);
    return pSource->save(path, pRoot);
}
