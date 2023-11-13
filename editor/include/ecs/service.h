#pragma once

#include "engine/core/panic.h"

#include "engine/service/service.h"

#if SM_DEBUG
#   define FLECS_DEBUG
#   define FLECS_SANITIZE
#   define FLECS_ACCURATE_COUNTERS
#endif

#pragma warning(push, 0)
#pragma warning(disable: 4127)

#include <flecs.h>

#pragma warning(pop)

namespace flecs {
    struct FlecsService final : simcoe::IStaticService<FlecsService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "ecs";
        static inline auto kServiceDeps = simcoe::depends();

        // IService
        bool createService() override;
        void destroyService() override;
    };
}
