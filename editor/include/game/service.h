#pragma once

#include "engine/service/service.h"

#include "game/ecs/world.h"

#include "game/render/hud.h"
#include "game/render/scene.h"
#include <random>

namespace game {
    struct GameService final : simcoe::IStaticService<GameService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "game";
        static inline auto kServiceDeps = simcoe::depends();

        // IService
        bool createService() override;
        void destroyService() override;

        // GameService
        static void setup(render::HudPass *pHudPass, render::ScenePass *pScenePass);

        static render::HudPass *getHud();
        static render::ScenePass *getScene();

        static game::World& getWorld();
        static threads::WorkQueue& getWorkQueue();
        static mt::SharedMutex& getWorldMutex();

        static std::mt19937_64 &getRng();
    };
}
