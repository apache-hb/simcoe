#pragma once

#include "engine/service/platform.h"

namespace simcoe {
    struct ConfigService final : IStaticService<ConfigService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "config";
        static constexpr std::array kServiceDeps = { PlatformService::kServiceName };

        // IService
        bool createService() override;
        void destroyService() override;

        static void loadServiceConfig(std::string_view name);

    private:
        std::string configDir;
    };
}
