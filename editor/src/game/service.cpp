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
config::ConfigValue<size_t> cfgWorkSeed("game/world", "random_seed", "random seed for world work", 12345);

namespace {
    game_render::HudPass *pHudPass = nullptr;
    game_render::ScenePass *pScenePass = nullptr;

    game::World *pWorld = nullptr;

    threads::WorkQueue *pWorkQueue = nullptr;
    mt::SharedMutex *pWorldMutex = nullptr;

    std::mt19937_64 *pRng = nullptr;
}

bool GameService::createService() {
    pWorld = new game::World();
    pRng = new std::mt19937_64(cfgWorkSeed.getCurrentValue());
    pWorkQueue = new threads::WorkQueue(64);
    pWorldMutex = new mt::SharedMutex("game");
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

game::World& GameService::getWorld() {
    return *pWorld;
}

threads::WorkQueue& GameService::getWorkQueue() {
    return *pWorkQueue;
}

mt::SharedMutex& GameService::getWorldMutex() {
    return *pWorldMutex;
}

std::mt19937_64 &GameService::getRng() {
    return *pRng;
}
