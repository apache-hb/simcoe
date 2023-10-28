#include "game/game.h"

#include "editor/graph/mesh.h"

using namespace game;

static Instance *gInstance = nullptr;
Instance* game::getInstance() { return gInstance; }
void game::setInstance(Instance *pInstance) { gInstance = pInstance; }

Instance::Instance(render::Graph *pGraph)
    : pGraph(pGraph)
{
    pDefaultMesh = newObjMesh("default.model");
    pDefaultTexture = newTexture("default.png");
}

Instance::~Instance() {
    quit();
}

void Instance::setupGame() {

}

void Instance::updateGame() {
    if (pGameQueue->process()) {
        return;
    }

    tick(gameUpdateStep.tick());
}

void Instance::setupRender() {
    LOG_INFO("render thread fault limit: {}", renderFaultLimit);
}

void Instance::updateRender() {
    if (pRenderQueue->process()) {
        return;
    }

    try {
        renderUpdateStep.tick();
        pGraph->execute();
    } catch (std::runtime_error& err) {
        renderFaultCount += 1;
        LOG_ERROR("render fault. {} total fault{}", renderFaultCount, renderFaultCount > 1 ? "s" : "");

        if (renderFaultCount >= renderFaultLimit) {
            LOG_ERROR("render fault exceeded limit of {}. exiting...", renderFaultLimit);
        }

        LOG_ERROR("attempting to recover...");
        pGraph->resumeFromFault();
    }
}

///
/// state machine
///

void Instance::pushLevel(Level *pLevel) {
    pGameQueue->add("push-level", [this, pLevel] {
        if (Level *pCurrent = getActiveLevel()) {
            pCurrent->pause();
        }

        pLevel->resume();

        std::lock_guard lock(gameMutex);
        levels.push_back(pLevel);
    });
}

void Instance::popLevel() {
    pGameQueue->add("pop-level", [this] {
        if (Level *pCurrent = getActiveLevel()) {
            pCurrent->pause();

            std::lock_guard lock(gameMutex);
            levels.pop_back();
        }

        if (Level *pCurrent = getActiveLevel()) {
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
    if (bPaused) return;

    if (Level *pCurrent = getActiveLevel()) {
        pCurrent->beginTick();
        pCurrent->tick(delta * timeScale);
        pCurrent->endTick();
    }
}

///
/// debug
///

void Instance::debug() {
    ImGui::Checkbox("Paused", &bPaused);

    ImGui::SliderFloat("Time Scale", &timeScale, 0.f, 2.f);
    ImGui::Text("Current Time: %f", clock.now());

    if (Level *pCurrent = getActiveLevel()) {
        auto name = pCurrent->getName();
        ImGui::SeparatorText(name.data());
        pCurrent->debug();
    }
}
