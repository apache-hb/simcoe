#pragma once

#include "engine/tasks/task.h"
#include "engine/util/time.h"

#include "game/level.h"

#include "editor/graph/assets.h"

namespace editor::game {
    using namespace simcoe;
    using namespace editor;

    namespace fs = assets::fs;

    struct Instance {
        Instance(Graph *pGraph);
        ~Instance();

        // physics thread
        tasks::WorkQueue *pPhysicsQueue = new tasks::WorkQueue{64};
        util::TimeStep physicsUpdateStep{1.f / 30.f};
        void setupPhysics();
        void updatePhysics();

        // game thread
        tasks::WorkQueue *pGameQueue = new tasks::WorkQueue{64};
        util::TimeStep gameUpdateStep{1.f / 60.f};
        void setupGame();
        void updateGame();

        // render thread
        tasks::WorkQueue *pRenderQueue = new tasks::WorkQueue{64};
        util::TimeStep renderUpdateStep{1.f / 240.f};
        void setupRender();
        void updateRender();
    private:
        size_t renderFaultLimit = 3;
        size_t renderFaultCount = 0;

    public:
        ///
        /// state machine
        ///
        void pushLevel(GameLevel *pLevel);
        void popLevel();

        void quit();
        bool shouldQuit() const { return bShouldQuit; }

        GameLevel *getActiveLevel() {
            std::lock_guard lock(gameMutex);
            return levels.empty() ? nullptr : levels.back();
        }

    private:
        std::mutex gameMutex;
        std::atomic_bool bShouldQuit = false;
        std::vector<GameLevel*> levels;


        ///
        /// rendering
        ///
    public:
        void loadMesh(const fs::path& path, std::function<void(render::IMeshBufferHandle*)> callback);
        void loadTexture(const fs::path& path, std::function<void(ResourceWrapper<graph::TextureHandle>*)> callback);

        render::IMeshBufferHandle *getDefaultMesh() { return pDefaultMesh; }
        ResourceWrapper<graph::TextureHandle> *getDefaultTexture() { return pDefaultTexture; }

    private:
        graph::ObjMesh *newObjMesh(const fs::path& path);
        ResourceWrapper<graph::TextureHandle> *newTexture(const fs::path& path);

        render::IMeshBufferHandle *pDefaultMesh = nullptr;
        ResourceWrapper<graph::TextureHandle> *pDefaultTexture = nullptr;

        std::unordered_map<fs::path, render::IMeshBufferHandle*> meshes;
        std::unordered_map<fs::path, ResourceWrapper<graph::TextureHandle>*> textures;

        Graph *pGraph = nullptr;

        ///
        /// time management
        ///
    public:
        void tick(float delta);

    private:
        Clock clock;
        bool bPaused = false;
        float timeScale = 1.f;

        ///
        /// debug
        ///
        void debug();
        debug::GlobalHandle debugHandle = debug::addGlobalHandle("Game", [this] { debug(); });
    };

    Instance* getInstance();
    void setInstance(Instance *pInstance);
}
