#include "editor/game/game.h"

#include "editor/graph/mesh.h"

using namespace editor;
using namespace editor::game;

static Instance *gInstance = nullptr;
Instance* game::getInstance() { return gInstance; }
void game::setInstance(Instance *pInstance) { gInstance = pInstance; }

Instance::Instance(render::Graph *pGraph)
    : tasks::WorkThread(64, "game")
    , pGraph(pGraph)
{
    pDefaultMesh = newObjMesh("default.model");
    pDefaultTexture = newTexture("default.png");
}

Instance::~Instance() {
    quit();
    stop();
}

// tasks::WorkThread
void Instance::run(std::stop_token token) {
    assetLock.migrate();
    gameLock.migrate();

    while (!token.stop_requested()) {
        if (process()) {
            continue;
        }

        if (bShouldQuit) break;

        tick(updateRate.tick() * timeScale);
    }
}

///
/// state machine
///

void Instance::pushLevel(GameLevel *pLevel) {
    gameLock.verify();
    if (GameLevel *pCurrent = getActiveLevel()) {
        pCurrent->pause();
    }

    pLevel->resume();

    std::lock_guard lock(gameMutex);
    levels.push_back(pLevel);
}

void Instance::popLevel() {
    gameLock.verify();

    if (GameLevel *pCurrent = getActiveLevel()) {
        pCurrent->pause();

        std::lock_guard lock(gameMutex);
        levels.pop_back();
    }

    if (GameLevel *pCurrent = getActiveLevel()) {
        pCurrent->resume();
    }
}

void Instance::quit() {
    bShouldQuit = true;
}

///
/// assets
///

void Instance::loadMesh(const fs::path& path, std::function<void(render::IMeshBufferHandle*)> callback) {
    add("load-mesh", [this, path, callback] {
        assetLock.verify();
        if (auto it = meshes.find(path); it != meshes.end()) {
            callback(it->second);
            return;
        }

        auto *pMesh = newObjMesh(path);
        meshes.emplace(path, pMesh);
        callback(pMesh);
    });
}

void Instance::loadTexture(const fs::path& path, std::function<void(ResourceWrapper<graph::TextureHandle>*)> callback) {
    add("load-texture", [this, path, callback] {
        assetLock.verify();
        if (auto it = textures.find(path); it != textures.end()) {
            callback(it->second);
            return;
        }

        auto *pTexture = newTexture(path);
        textures.emplace(path, pTexture);
        callback(pTexture);
    });
}

graph::ObjMesh *Instance::newObjMesh(const fs::path& path) {
    const auto& createInfo = pGraph->getCreateInfo();
    const auto assetPath = createInfo.depot.getAssetPath(path);

    return pGraph->newGraphObject<graph::ObjMesh>(assetPath);
}

ResourceWrapper<graph::TextureHandle> *Instance::newTexture(const fs::path& path) {
    return pGraph->addResource<graph::TextureHandle>(path.string());
}

///
/// time management
///

void Instance::tick(float delta) {
    gameLock.verify();
    lastTick = delta;

    if (GameLevel *pCurrent = getActiveLevel()) {
        pCurrent->beginTick();
        pCurrent->tick(delta * timeScale);
        pCurrent->endTick();
    }
}

///
/// debug
///

void Instance::debug() {
    ImGui::SliderFloat("Time Scale", &timeScale, 0.f, 2.f);
    ImGui::Text("Updates per second: %f", 1.f / lastTick);
    ImGui::Text("Update Rate: %f", updateRate.getDelta());
    ImGui::Text("Current Time: %f", clock.now());

    if (GameLevel *pCurrent = getActiveLevel()) {
        ImGui::SeparatorText(pCurrent->name);
        pCurrent->debug();
    }
}
