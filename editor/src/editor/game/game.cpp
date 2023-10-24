#include "editor/game/game.h"

#include "editor/graph/mesh.h"

using namespace editor;
using namespace editor::game;

static Instance *gInstance = nullptr;
Instance* game::getInstance() { return gInstance; }
void game::setInstance(Instance *pInstance) { gInstance = pInstance; }

Instance::Instance(render::Graph *pGraph)
    : pGraph(pGraph)
{

}

Instance::~Instance() {
    quit();
}

void Instance::setupGame() {
    pDefaultMesh = newObjMesh("default.model");
    pDefaultTexture = newTexture("default.png");
}

void Instance::updateGame() {
    pGameQueue->process();

    float now = clock.now();
    float delta = now - lastTime;
    lastTime = now;
    tick(delta);
}

void Instance::setupRender() {

}

void Instance::updateRender() {
    pRenderQueue->process();

    pGraph->execute();
}

///
/// state machine
///

void Instance::pushLevel(GameLevel *pLevel) {
    pGameQueue->add("push-level", [this, pLevel] {
        if (GameLevel *pCurrent = getActiveLevel()) {
            pCurrent->pause();
        }

        pLevel->resume();

        std::lock_guard lock(gameMutex);
        levels.push_back(pLevel);
    });
}

void Instance::popLevel() {
    pGameQueue->add("pop-level", [this] {
        if (GameLevel *pCurrent = getActiveLevel()) {
            pCurrent->pause();

            std::lock_guard lock(gameMutex);
            levels.pop_back();
        }

        if (GameLevel *pCurrent = getActiveLevel()) {
            pCurrent->resume();
        }
    });
}

void Instance::quit() {
    bShouldQuit = true;
}

///
/// assets
///

void Instance::loadMesh(const fs::path& path, std::function<void(render::IMeshBufferHandle*)> callback) {
    pRenderQueue->add("load-mesh", [this, path, callback] {
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
    pRenderQueue->add("load-texture", [this, path, callback] {
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
    ImGui::Text("Current Time: %f", clock.now());

    if (GameLevel *pCurrent = getActiveLevel()) {
        ImGui::SeparatorText(pCurrent->name);
        pCurrent->debug();
    }
}
