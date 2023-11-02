#pragma once

#include "editor/graph/assets.h"

namespace editor::graph {
    struct PostPass final : IRenderPass {
        PostPass(
            Graph *pGraph,
            ResourceWrapper<IRTVHandle> *pRenderTarget,
            ResourceWrapper<ISRVHandle> *pSceneSource
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
        PresentPass(Graph *pGraph, ResourceWrapper<SwapChainHandle> *pBackBuffers);

        void create() override { }
        void destroy() override { }
        void execute() override { }

    private:
        PassAttachment<SwapChainHandle> *pBackBuffers;
    };
}
