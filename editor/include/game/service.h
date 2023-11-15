#pragma once

#include "engine/service/service.h"

#include "game/render/hud.h"
#include "game/render/scene.h"

namespace game {
    struct GameService final : simcoe::IStaticService<GameService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "game";
        static inline auto kServiceDeps = simcoe::depends();

        // IService
        bool createService() override;
        void destroyService() override;

        // GameService
        static void setup(graph::HudPass *pHudPass, graph::ScenePass *pScenePass);

        static graph::HudPass *getHud();
        static graph::ScenePass *getScene();
    };
}
