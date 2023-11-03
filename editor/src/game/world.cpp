#include "game/world.h"

#include "game/entity.h"

#include "engine/input/input.h"

#include "engine/render/graph.h"

using namespace game;

World::World(const WorldInfo& info)
    : rng(info.seed)
    , info(info)
{ }

// render

void World::destroyRender() {
    delete pRenderThread;

    delete info.pRenderGraph;
    delete info.pRenderContext;
}

void World::tickRender() {
    if (pRenderThread->process()) {
        return;
    }

    render::Graph *pGraph = info.pRenderGraph;

    try {
        pGraph->execute();
        renderStep.waitForNextTick();
    } catch (const std::runtime_error& e) {
        renderFaults += 1;
        LOG_ERROR("fault: {}", e.what());
        LOG_ERROR("render fault. {} total fault{}", renderFaults, renderFaults > 1 ? "s" : "");

        if (renderFaults >= info.renderFaultLimit) {
            LOG_ERROR("render fault exceeded limit of {}. exiting...", info.renderFaultLimit);
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
    if (pPhysicsThread->process()) {
        return;
    }

    physicsStep.waitForNextTick();
}

// game

void World::destroyGame() {
    delete pGameThread;
}

void World::tickGame() {
    if (pGameThread->process()) {
        return;
    }

    gameStep.waitForNextTick();
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
