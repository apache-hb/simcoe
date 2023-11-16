#pragma once

#include "engine/render/graph.h"

#include "editor/graph/assets.h"

namespace game::render {
    using namespace editor::graph;
    using namespace simcoe::render;
    
    struct HudPass final : IRenderPass {
        HudPass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget);

        void create() override;
        void destroy() override;

        void execute() override;
    };
}
