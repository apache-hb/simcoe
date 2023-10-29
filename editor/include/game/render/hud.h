#pragma once

#include "engine/render/graph.h"

namespace game::graph {
    using namespace simcoe::render;

    struct HudPass final : IRenderPass {
        HudPass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget);

        void create() override;
        void destroy() override;

        void execute() override;

    private:

    };
}
