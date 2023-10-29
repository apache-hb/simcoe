#include "game/world.h"

#include "game/entity.h"

#include "engine/input/input.h"

#include "engine/render/graph.h"

using namespace game;

World::World(const WorldInfo& info)
    : rng(info.seed)
    , info(info)
{ }

// input

void World::destroyInput() {
    delete pInputThread;
}

void World::tickInput() {
    if (pInputThread->process()) {
        return;
    }

    input::Manager *pInput = info.pInput;
    pInput->poll();
}

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
    } catch (const std::runtime_error& e) {
        renderFaults += 1;
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

}

// game

void World::destroyGame() {
    delete pGameThread;
}

void World::tickGame() {

}

// correct shutdown order is:
// - shutdown game thread
// - shutdown physics thread
// - shutdown render thread
// - shutdown input thread
void World::shutdown() {
    std::lock_guard guard(lock);
    // TODO: wait for everything to shut down

    destroyGame();
    destroyPhysics();
    destroyRender();
    destroyInput();

    bShutdown = true;
}

EntityVersion World::newEntityVersion() {
    std::lock_guard guard(lock);
    return EntityVersion(dist(rng));
}
