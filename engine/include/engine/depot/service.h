#pragma once

#include "engine/service/service.h"

#include <array>

namespace simcoe {
    struct DepotService final : IStaticService<DepotService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "depot";
        static constexpr std::array<std::string_view, 0> kServiceDeps = { };

        // IService
        bool createService() override;
        void destroyService() override;

        static void setup();

        // failure
        static std::string_view getFailureReason();

    private:
        std::string failureReason;

        std::string depotPath;
    };
}
