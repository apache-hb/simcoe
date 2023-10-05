#pragma once

#include "editor/graph/assets.h"

#include "engine/os/system.h"

namespace editor::graph {
    struct UniformHandle final : IUniformHandle {
        UniformHandle(RenderContext *ctx)
            : IUniformHandle(ctx)
        { }

        void create() override;
        void destroy() override;

        void update(RenderContext *ctx);

        RenderTargetAlloc::Index getRtvIndex() const override {
            return RenderTargetAlloc::Index::eInvalid;
        }
    private:
        simcoe::Timer timer;
    };

    struct ScenePass final : IRenderPass {
        ScenePass(RenderContext *ctx, graph::SceneTargetHandle *pSceneTarget, graph::TextureHandle *pTexture, graph::UniformHandle *pUniform);

        void create() override;
        void destroy() override;
        void execute() override;

    private:

        PassResource<graph::SceneTargetHandle> *pSceneTarget;
        PassResource<graph::TextureHandle> *pTextureHandle;
        PassResource<graph::UniformHandle> *pUniformHandle;

        render::Display display;

        render::PipelineState *pPipeline;

        render::VertexBuffer *pQuadVertexBuffer;
        render::IndexBuffer *pQuadIndexBuffer;
    };
}
