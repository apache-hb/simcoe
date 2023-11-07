#pragma once

#include "game/entity.h"
#include "game/tick.h"

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


        void createPhysics() { }
        void destroyPhysics();
        void tickPhysics();
        float lastPhysicsTime = 0.f;
        threads::WorkQueue *pPhysicsThread = new threads::WorkQueue{64};


        void createGame() { }
        void destroyGame();
        void tickGame();
        float lastGameTime = 0.f;
        threads::WorkQueue *pGameThread = new threads::WorkQueue{64};


        template<std::derived_from<ILevel> T, typename... A> requires std::is_constructible_v<T, LevelInfo, A...>
        T *addLevel(A&&... args) {
            std::lock_guard guard(lock);

            const LevelInfo levelInfo = {
                .entityLimit = info.entityLimit,
                .pWorld = this
            };

            return new T(levelInfo, args...);
        }

        void setCurrentLevel(ILevel *pLevel);

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

        // level bookkeeping
        std::unordered_set<ILevel*> levels;
        ILevel *pActiveLevel = nullptr;

        // render bookkeeping
        size_t renderFaults = 0;

        // config
        game::WorldInfo info;
    };
}
