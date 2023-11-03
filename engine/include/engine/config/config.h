#pragma once

#include "engine/config/schema.h"

namespace simcoe::config {
    struct ConfigFile {
        ConfigFile(const fs::path& path);

        void load(const ISchemaBase *pConfig, void *pObjectPtr) const;

    private:
        std::string name;
        toml::table config;

        static toml::table loadFile(const fs::path& path);
    };
}
