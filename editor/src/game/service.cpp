#include "game/service.h"

#include "engine/log/service.h"
#include "engine/render/service.h"

#include "game/render/hud.h"
#include "game/render/scene.h"

#include "engine/config/system.h"

using namespace game;
using namespace simcoe;

config::ConfigValue<float> cfgTargetWorldTickRate("game/world", "target_tps", "target world ticks per second (0 = unlimited)", 30.f);

namespace {
    graph::HudPass *pHudPass = nullptr;
    graph::ScenePass *pScenePass = nullptr;
}

bool GameService::createService() {
    return true;
}

void GameService::destroyService() {

}

void GameService::setup(graph::HudPass *pNewHudPass, graph::ScenePass *pNewScenePass) {
    pHudPass = pNewHudPass;
    pScenePass = pNewScenePass;
}

graph::HudPass *GameService::getHud() {
    return pHudPass;
}

graph::ScenePass *GameService::getScene() {
    return pScenePass;
}
