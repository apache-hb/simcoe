#pragma once

#include "engine/render/graph.h"

#include "engine/threads/mutex.h"

#include <unordered_set>

namespace game::graph {
    using namespace simcoe::render;

    struct ScenePass final : IRenderPass {
        ScenePass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<IDSVHandle> *pDepthTarget);

        // IRenderPass
        void create() override;
        void destroy() override;

        void execute() override;
    };
}
