#pragma once

#include "engine/service/service.h"

#include "engine/depot/service.h"

namespace simcoe {
    struct AudioService final : IStaticService<AudioService> {
        // IStaticService
        constexpr static std::string_view kServiceName = "audio";
        constexpr static std::array<std::string_view, 0> kServiceDeps = { DepotService::kServiceName };

        // IService
        bool createService() override;
        void destroyService() override;
    };
}
