#pragma once

#include "editor/graph/assets.h"

namespace editor::graph {
    struct PostPass final : IRenderPass {
        PostPass(
            Context *ctx,
            ResourceWrapper<ISRVHandle> *pSceneTarget,
            ResourceWrapper<IRTVHandle> *pBackBuffers
        );

        void create() override;
        void destroy() override;
        void execute() override;

    private:
        PassAttachment<ISRVHandle> *pSceneTarget;
        PassAttachment<IRTVHandle> *pBackBuffers;

        rhi::Display display;
        rhi::PipelineState *pPipeline;

        rhi::VertexBuffer *pScreenQuadVerts;
        rhi::IndexBuffer *pScreenQuadIndices;
    };

    struct PresentPass final : IRenderPass {
        PresentPass(
            Context *ctx,
            ResourceWrapper<IRTVHandle> *pBackBuffers
        );

        void create() override;
        void destroy() override;
        void execute() override;

    private:
        PassAttachment<IRTVHandle> *pBackBuffers;
    };
}
