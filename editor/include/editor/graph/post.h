#pragma once

#include "editor/graph/assets.h"

namespace editor::graph {
    struct PostPass final : IRenderPass {
        PostPass(
            Graph *ctx,
            ResourceWrapper<ISRVHandle> *pSceneSource,
            ResourceWrapper<IRTVHandle> *pRenderTarget
        );

        void create() override;
        void destroy() override;
        void execute() override;

    private:
        PassAttachment<ISRVHandle> *pSceneSource;

        rhi::Display display;
        rhi::PipelineState *pPipeline;

        rhi::VertexBuffer *pScreenQuadVerts;
        rhi::IndexBuffer *pScreenQuadIndices;
    };

    struct PresentPass final : ICommandPass {
        PresentPass(Graph *ctx, ResourceWrapper<SwapChainHandle> *pBackBuffers);

        void create() override;
        void destroy() override;
        void execute() override;

    private:
        PassAttachment<SwapChainHandle> *pBackBuffers;
    };
}
