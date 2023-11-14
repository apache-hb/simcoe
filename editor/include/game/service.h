#pragma once

#include "engine/service/service.h"

#include "vendor/flecs/flecs.h"

namespace game {
    struct GameService final : simcoe::IStaticService<GameService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "game";
        static inline auto kServiceDeps = simcoe::depends();

        // IService
        bool createService() override;
        void destroyService() override;

        // GameService
        static flecs::world &getWorld();

        static void progress();
    };
}
