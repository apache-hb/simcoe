#include "game/service.h"

#include "engine/log/service.h"
#include "engine/render/service.h"

#include "game/render/hud.h"
#include "game/render/scene.h"

using namespace game;
using namespace simcoe;

namespace {
    flecs::world *pWorld = nullptr;

    static log::Level getEcsLevel(int32_t level) {
        switch (level) {
        case -4: return log::eAssert;
        case -3: return log::eError;
        case -2: return log::eWarn;
        case -1: return log::eInfo;
        case 0: return log::eDebug;
        default: return log::eDebug;
        }
    }

    graph::HudPass *pHudPass = nullptr;
    graph::ScenePass *pScenePass = nullptr;
}

bool GameService::createService() {
    // hook our backtrace support into flecs
    ecs_os_init();

    ecs_os_api_t api = ecs_os_get_api();
    api.abort_ = [](void) { SM_NEVER("flecs error"); };
    api.log_ = [](int32_t level, const char *file, int32_t line, const char *msg) {
        LoggingService::sendMessage(getEcsLevel(level), std::format("{}:{}: {}", file, line, msg));
    };

    ecs_os_set_api(&api);

    pWorld = new flecs::world();

    pWorld->set_target_fps(30.f);
    return true;
}

void GameService::destroyService() {
    delete pWorld;
}

void GameService::setup(graph::HudPass *pNewHudPass, graph::ScenePass *pNewScenePass) {
    pHudPass = pNewHudPass;
    pScenePass = pNewScenePass;
}

flecs::world& GameService::getWorld() {
    return *pWorld;
}

void GameService::progress() {
    pWorld->progress();
}
