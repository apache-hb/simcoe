#include "game/world.h"

#include "game/entity.h"

#include "engine/input/input.h"
#include "engine/config/system.h"

#include "engine/render/graph.h"

using namespace simcoe;
using namespace game;

using namespace std::chrono_literals;

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
    // if (pRenderQueue->tryGetMessage()) {
    //     return;
    // }

    // render::Graph *pGraph = info.pRenderGraph;

    // try {
    //     pGraph->execute();
    // } catch (const core::Error& e) {
    //     if (!e.recoverable()) {
    //         throw;
    //     }

    //     renderFaults += 1;
    //     LOG_ERROR("fault: {}", e.what());
    //     LOG_ERROR("render fault. {} total fault{}", renderFaults, renderFaults > 1 ? "s" : "");

    //     auto faultLimit = cfgRenderFaultLimit.getCurrentValue();
    //     if (renderFaults >= faultLimit) {
    //         LOG_ERROR("render fault exceeded limit of {}. exiting...", faultLimit);
    //         throw;
    //     }

    //     LOG_ERROR("attempting to recover...");
    //     pGraph->resumeFromFault();
    // }
}


// game

// correct shutdown order is:
// - shutdown render thread
void World::shutdown() {
    std::lock_guard guard(lock);
    // TODO: wait for everything to shut down

    destroyRender();

    bShutdown = true;
}

EntityVersion World::newEntityVersion() {
    std::lock_guard guard(lock);
    return EntityVersion(dist(rng));
}
