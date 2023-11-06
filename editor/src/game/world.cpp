#include "game/world.h"

#include "game/entity.h"

#include "engine/input/input.h"

#include "engine/render/graph.h"

using namespace std::chrono_literals;

using namespace game;

World::World(const WorldInfo& info)
    : rng(info.seed)
    , info(info)
{ }

// render

void World::destroyRender() {
    delete pRenderQueue;

    delete info.pRenderGraph;
    delete info.pRenderContext;
}

void World::tickRender() {
    if (pRenderQueue->tryGetMessage()) {
        return;
    }

    render::Graph *pGraph = info.pRenderGraph;

    try {
        pGraph->execute();
    } catch (const std::runtime_error& e) {
        renderFaults += 1;
        LOG_ERROR("fault: {}", e.what());
        LOG_ERROR("render fault. {} total fault{}", renderFaults, renderFaults > 1 ? "s" : "");

        if (renderFaults >= info.renderFaultLimit) {
            LOG_ERROR("render fault exceeded limit of {}. exiting...", info.renderFaultLimit);
            throw;
        }

        LOG_ERROR("attempting to recover...");
        pGraph->resumeFromFault();
    }
}

// physics

void World::destroyPhysics() {
    delete pPhysicsThread;
}

void World::tickPhysics() {
    if (pPhysicsThread->tryGetMessage()) {
        return;
    }

    std::this_thread::sleep_for(16ms);
}

// game

void World::destroyGame() {
    delete pGameThread;
}

void World::tickGame() {
    if (pGameThread->tryGetMessage()) {
        return;
    }

    std::this_thread::sleep_for(16ms);
}

// correct shutdown order is:
// - shutdown game thread
// - shutdown physics thread
// - shutdown render thread
void World::shutdown() {
    std::lock_guard guard(lock);
    // TODO: wait for everything to shut down

    destroyGame();
    destroyPhysics();
    destroyRender();

    bShutdown = true;
}

EntityVersion World::newEntityVersion() {
    std::lock_guard guard(lock);
    return EntityVersion(dist(rng));
}
