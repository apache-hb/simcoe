#pragma once

#include "editor/graph/assets.h"

#include "engine/system/system.h"

namespace editor::graph {
    struct ScenePass final : IRenderPass {
        ScenePass(Graph *ctx, ResourceWrapper<IRTVHandle> *pRenderTarget);

        void create() override;
        void destroy() override;
        void execute() override;

    private:
        rhi::Display display;
    };
}
