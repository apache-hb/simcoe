#pragma once

#include "game/entity.h"

#include "engine/tasks/task.h"

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


        void createInput();
        void destroyInput();
        void tickInput();
        tasks::WorkThread *pInputThread = new tasks::WorkThread{64, "input"};


        void createRender();
        void destroyRender();
        void tickRender();
        tasks::WorkThread *pRenderThread = new tasks::WorkThread{64, "render"};


        void createPhysics();
        void destroyPhysics();
        void tickPhysics();
        tasks::WorkThread *pPhysicsThread = new tasks::WorkThread{64, "physics"};


        void createGame();
        void destroyGame();
        void tickGame();
        tasks::WorkThread *pGameThread = new tasks::WorkThread{64, "game"};


        template<std::derived_from<ILevel> T, typename... A> requires std::is_constructible_v<T, LevelInfo, A...>
        T *addLevel(A&&... args) {
            std::lock_guard guard(lock);

            LevelInfo info = {
                .entityLimit = entityLimit,
                .pWorld = this
            };

            return new T(info, args...);
        }

        void setCurrentLevel(ILevel *pLevel);

        void shutdown();
        bool shouldQuit() const;

        EntityVersion newEntityVersion();
    private:
        // game engine lock to ensure thread safety
        std::mutex lock;

        // rng for generating entity ids
        std::mt19937_64 rng;
        std::uniform_int_distribution<EntityVersionType> dist{ 0, EntityVersionType(EntityVersion::eInvalid) - 1 };

        // level bookkeeping
        std::unordered_set<ILevel*> levels;
        ILevel *pActiveLevel = nullptr;

        // backend info
        size_t entityLimit;
        render::HudPass *pHudPass = nullptr;
        render::ScenePass *pScenePass = nullptr;
    };
}
