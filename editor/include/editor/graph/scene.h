#pragma once

#include "editor/graph/assets.h"

namespace editor::graph {
    struct ScenePass final : IRenderPass {
        ScenePass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget);

        void create() override;
        void destroy() override;
        void execute() override;

    private:
        rhi::Display display;
    };
}
