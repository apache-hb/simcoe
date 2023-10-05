#pragma once

#include "editor/render/graph.h"

#include "editor/graph/assets.h"

#include <array>

namespace editor::graph {
    struct PostPass final : IRenderPass {
        PostPass(RenderContext *ctx, graph::SceneTargetHandle *pSceneTarget, graph::SwapChainHandle *pBackBuffers);

        void create() override;
        void destroy() override;
        void execute() override;

    private:
        PassResource<graph::SceneTargetHandle> *pSceneTarget;
        PassResource<graph::SwapChainHandle> *pBackBuffers;

        render::Display display;
        render::PipelineState *pPipeline;

        render::VertexBuffer *pScreenQuadVerts;
        render::IndexBuffer *pScreenQuadIndices;
    };

    struct PresentPass final : IRenderPass {
        PresentPass(RenderContext *ctx, graph::SwapChainHandle *pBackBuffers);

        void create() override;
        void destroy() override;
        void execute() override;

    private:
        PassResource<graph::SwapChainHandle> *pBackBuffers;
    };
}
