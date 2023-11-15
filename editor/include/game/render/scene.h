#pragma once

#include "engine/render/assets.h"
#include "editor/graph/assets.h"

#include "engine/render/graph.h"

#include "engine/threads/mutex.h"

#include <unordered_set>

namespace game::graph {
    using namespace editor::graph;
    using namespace simcoe::math;
    using namespace simcoe::render;

    using SceneAction = std::function<void(Graph*, Context*)>;

    struct UNIFORM_BUFFER Model {
        float4x4 model;
    };

    struct UNIFORM_BUFFER Camera {
        float4x4 view;
        float4x4 proj;
    };

    struct CommandBatch {
        std::vector<SceneAction> actions;
    };

    struct ScenePass final : IRenderPass {
        ScenePass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<IDSVHandle> *pDepthTarget);

        // IRenderPass
        void create() override;
        void destroy() override;

        void execute() override;

        void update(CommandBatch&& updateBatch);

    private:
        rhi::Display display;

        CommandBatch batch;

        CommandBatch newBatch;
        bool bDirtyBatch = false;
        mt::Mutex lock{"batch"};
    };
}
