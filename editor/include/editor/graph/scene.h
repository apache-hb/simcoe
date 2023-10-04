#pragma once

#include "editor/graph/assets.h"

#include "engine/os/system.h"

namespace editor::graph {
    struct UniformHandle final : IUniformHandle {
        void create(RenderContext *ctx) override;
        void destroy(RenderContext *ctx) override;

        void update(RenderContext *ctx);

        RenderTargetAlloc::Index getRtvIndex(RenderContext *ctx) const override {
            return RenderTargetAlloc::Index::eInvalid;
        }
    private:
        simcoe::Timer timer;
    };

    struct ScenePass final : IRenderPass {
        ScenePass(graph::SceneTargetHandle *pSceneTarget, graph::TextureHandle *pTexture, graph::UniformHandle *pUniform);

        void create(RenderContext *ctx) override;
        void destroy(RenderContext *ctx) override;
        void execute(RenderContext *ctx) override;

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
