#pragma once

#include "editor/graph/assets.h"

#include "engine/os/system.h"

namespace editor::graph {
    struct UniformHandle final : IUniformHandle {
        UniformHandle(Context *ctx)
            : IUniformHandle(ctx, "uniform")
        { }

        void create() override;

        void update(Context *ctx);
    private:
        simcoe::Timer timer;
    };

    struct ScenePass final : IRenderPass {
        ScenePass(Context *ctx, graph::SceneTargetHandle *pSceneTarget, graph::TextureHandle *pTexture, graph::UniformHandle *pUniform);

        void create() override;
        void destroy() override;
        void execute() override;

    private:
        PassAttachment<graph::SceneTargetHandle> *pSceneTarget;
        PassAttachment<graph::TextureHandle> *pTextureHandle;
        PassAttachment<graph::UniformHandle> *pUniformHandle;

        rhi::Display display;

        rhi::PipelineState *pPipeline;

        rhi::VertexBuffer *pQuadVertexBuffer;
        rhi::IndexBuffer *pQuadIndexBuffer;
    };
}
