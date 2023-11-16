#include "game/service.h"

#include "engine/log/service.h"
#include "engine/render/service.h"

#include "game/render/hud.h"
#include "game/render/scene.h"

#include "engine/config/system.h"

using namespace game;

namespace game_render = game::render;
namespace config = simcoe::config;

config::ConfigValue<float> cfgTargetWorldTickRate("game/world", "target_tps", "target world ticks per second (0 = unlimited)", 30.f);

namespace {
    game_render::HudPass *pHudPass = nullptr;
    game_render::ScenePass *pScenePass = nullptr;
}

bool GameService::createService() {
    return true;
}

void GameService::destroyService() {

}

void GameService::setup(game_render::HudPass *pNewHudPass, game_render::ScenePass *pNewScenePass) {
    pHudPass = pNewHudPass;
    pScenePass = pNewScenePass;
}

game_render::HudPass *GameService::getHud() {
    return pHudPass;
}

game_render::ScenePass *GameService::getScene() {
    return pScenePass;
}
