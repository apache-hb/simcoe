#pragma once

#include "engine/render/graph.h"

#include <unordered_set>

namespace game::graph {
    using namespace simcoe::render;

    struct ISceneObject {
        virtual ~ISceneObject() = default;

        virtual void execute(Graph *pGraph);
    };

    struct ScenePass final : IRenderPass {
        ScenePass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<IDSVHandle> *pDepthTarget);

        // IRenderPass
        void create() override;
        void destroy() override;

        void execute() override;

        // public api

        void addSceneObject(ISceneObject *pObject);
        void removeSceneObject(ISceneObject *pObject);

    private:
        std::mutex lock;

        std::unordered_set<ISceneObject*> objects;
    };
}
