#pragma once

#include "editor/graph/assets.h"

namespace editor::graph {
    struct PostPass final : IRenderPass {
        PostPass(Context *ctx, graph::SceneTargetHandle *pSceneTarget, graph::SwapChainHandle *pBackBuffers);

        void create() override;
        void destroy() override;
        void execute() override;

    private:
        PassAttachment<graph::SceneTargetHandle> *pSceneTarget;
        PassAttachment<graph::SwapChainHandle> *pBackBuffers;

        rhi::Display display;
        rhi::PipelineState *pPipeline;

        rhi::VertexBuffer *pScreenQuadVerts;
        rhi::IndexBuffer *pScreenQuadIndices;
    };

    struct PresentPass final : IRenderPass {
        PresentPass(Context *ctx, graph::SwapChainHandle *pBackBuffers);

        void create() override;
        void destroy() override;
        void execute() override;

    private:
        PassAttachment<graph::SwapChainHandle> *pBackBuffers;
    };
}
