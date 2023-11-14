#pragma once

#include "game/entity.h"

#include "engine/threads/queue.h"
#include "engine/threads/mutex.h"

#include "engine/util/time.h"

#include <unordered_set>
#include <random>

namespace game {
    /**
     * all public methods are thread safe
     *
     * all internal methods are not thread safe
     */
    struct World {
        World(const WorldInfo& info);
        ~World();

        void createRender() { }
        void destroyRender();
        void tickRender();
        float lastRenderTime = 0.f;
        threads::WorkQueue *pRenderQueue = new threads::WorkQueue{64};

        void shutdown();
        bool shouldQuit() const { return bShutdown; }

        EntityVersion newEntityVersion();
    private:
        std::atomic_bool bShutdown = false;

        // game engine lock to ensure thread safety
        mt::Mutex lock{"world"};
        Clock clock;

        // rng for generating entity ids
        std::mt19937_64 rng;
        std::uniform_int_distribution<EntityVersionType> dist{ 0, EntityVersionType(EntityVersion::eInvalid) - 1 };

        // render fault tracking
        size_t renderFaults = 0;

        // config
        game::WorldInfo info;
    };
}
