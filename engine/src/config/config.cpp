#include "engine/config/config.h"

#include "engine/log/service.h"

using namespace simcoe;
using namespace simcoe::config;

ConfigFile::ConfigFile(const fs::path& path)
    : name(path.string())
    , config(loadFile(path))
{ }

void ConfigFile::load(std::string_view sectionName, const IConfig *pConfig) const {
    auto *pSchema = pConfig->getSchema();
    if (pSchema == nullptr) return;

    ConfigContext ctx{name};

    if (auto field = config.get(sectionName); field) {
        if (field->is_table()) {
            pSchema->load(ctx, *field->as_table());
        } else {
            LOG_WARN("config file {} section for {} is not a table", sectionName, sectionName);
        }
    } else {
        LOG_WARN("config file {} does not contain section for {}", sectionName, sectionName);
    }
}

toml::table ConfigFile::loadFile(const fs::path& path) try {
    fs::path cfg = fs::current_path() / path;
    cfg.replace_extension("toml");
    auto str = cfg.string();

    LOG_INFO("loading config file {}", str);
    return toml::parse_file(str);
} catch (const toml::parse_error& e) {
    std::stringstream ss; // TODO: maybe pester the toml library to add std::formatter support
    ss << e;
    LOG_WARN("failed to parse config file {}", path.string());
    LOG_WARN("{}", ss.str());
    return {};
} catch (const std::exception& e) {
    LOG_WARN("failed to load config file {}: {}", path.string(), e.what());
    return {};
}
