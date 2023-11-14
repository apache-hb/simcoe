#pragma once

#include "engine/render/graph.h"

#include "editor/graph/assets.h"

namespace game::graph {
    using namespace editor::graph;
    using namespace simcoe::render;

    struct Widget {

    };

    struct TextWidget : Widget {
        TextWidget(std::string text);
        
    private:
        std::string text;

        ResourceWrapper<TextureHandle> *pHandle = nullptr;
        PassAttachment<TextureHandle> *pAttachment = nullptr;
    };

    struct HudPass final : IRenderPass {
        HudPass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget);

        void create() override;
        void destroy() override;

        void execute() override;

    private:

    };
}
