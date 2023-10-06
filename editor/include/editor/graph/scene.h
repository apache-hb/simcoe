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
        SceneUniformHandle(Context *ctx)
            : IUniformHandle(ctx, "uniform")
        { }

        void update(Context *ctx);
    private:
        simcoe::Timer timer;
    };

    struct ScenePass final : IRenderPass {
        ScenePass(
            Context *ctx,
            ResourceWrapper<IRTVHandle> *pSceneTarget,
            ResourceWrapper<TextureHandle> *pTexture,
            ResourceWrapper<SceneUniformHandle> *pUniform
        );

        void create() override;
        void destroy() override;
        void execute() override;

    private:
        PassAttachment<IRTVHandle> *pSceneTarget;
        PassAttachment<TextureHandle> *pTextureHandle;
        PassAttachment<SceneUniformHandle> *pUniformHandle;

        rhi::Display display;

        rhi::PipelineState *pPipeline;

        rhi::VertexBuffer *pQuadVertexBuffer;
        rhi::IndexBuffer *pQuadIndexBuffer;
    };
}
