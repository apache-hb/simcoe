#pragma once

#include "engine/service/service.h"

#include "engine/core/filesystem.h"

#include <array>

namespace simcoe {
    struct ConfigService final : IStaticService<ConfigService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "config";
        static constexpr std::array<std::string_view, 0> kServiceDeps = { };

        // IService
        bool createService() override;
        void destroyService() override;

        // public api
        static bool loadConfig(const fs::path& path);
        static bool saveConfig(const fs::path& path, bool bModifiedOnly);
    };
}
