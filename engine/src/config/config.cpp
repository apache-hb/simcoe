#include "engine/config/config.h"

#include "engine/log/service.h"

using namespace simcoe;
using namespace simcoe::config;

ConfigFile::ConfigFile(const fs::path& path)
    : name(path.string())
{
    pSource = loadToml(path);
    pRoot = pSource ? pSource->load() : nullptr;
}

void ConfigFile::load(std::string_view sectionName, const IConfig *pConfig) const {
    if (pSource == nullptr) return;

    auto *pSchema = pConfig->getSchema();
    if (pSchema == nullptr) return;

    ConfigContext ctx{name};

    if (NodeMap sections; pRoot->get(sections)) {
        if (auto it = sections.find(std::string(sectionName)); it != sections.end()) {
            pSchema->load(ctx, it->second);
        } else {
            LOG_WARN("config file {} does not contain section for {}", name, sectionName);
        }
    } else {
        LOG_WARN("config file {} does not contain sections", name);
    }
}
