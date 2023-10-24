#pragma once

#include "engine/tasks/task.h"
#include "engine/util/time.h"

#include "editor/game/level.h"
#include "editor/graph/assets.h"

namespace editor::game {
    using namespace simcoe;
    using namespace editor;

    namespace fs = assets::fs;

    struct Instance {
        Instance(Graph *pGraph);
        ~Instance();

        tasks::WorkQueue *pGameQueue = new tasks::WorkQueue{64};
        void setupGame();
        void updateGame();

        tasks::WorkQueue *pRenderQueue = new tasks::WorkQueue{64};
        void setupRender();
        void updateRender();

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
        system::Clock clock;
        float lastTime = 0.f;
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
