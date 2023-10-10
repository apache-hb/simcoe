#pragma once

#include "editor/graph/assets.h"

#include "engine/os/system.h"

namespace editor::graph {
    struct UNIFORM_ALIGN UniformData {
        math::float2 offset;

        float angle;
        float aspect;
    };

    struct SceneUniformHandle final : IUniformHandle<UniformData> {
        SceneUniformHandle(Graph *ctx)
            : IUniformHandle(ctx, "uniform")
        { }

        void update();
    private:
        simcoe::Timer timer;
    };

    struct ScenePass final : IRenderPass {
        ScenePass(
            Graph *ctx,
            ResourceWrapper<IRTVHandle> *pRenderTarget,
            ResourceWrapper<TextureHandle> *pTexture,
            ResourceWrapper<SceneUniformHandle> *pUniform
        );

        void create() override;
        void destroy() override;
        void execute() override;

    private:
        PassAttachment<TextureHandle> *pTextureHandle;
        PassAttachment<SceneUniformHandle> *pUniformHandle;

        rhi::Display display;

        rhi::PipelineState *pPipeline;

        rhi::VertexBuffer *pQuadVertexBuffer;
        rhi::IndexBuffer *pQuadIndexBuffer;
    };
}
