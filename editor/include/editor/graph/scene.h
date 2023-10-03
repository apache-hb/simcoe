#pragma once

#include "editor/graph/assets.h"

#include "engine/os/system.h"

namespace editor::graph {

    struct UNIFORM_ALIGN UniformData {
        math::float2 offset;

        float angle;
        float aspect;
    };

    struct ScenePass final : IRenderPass {
        ScenePass(graph::SceneTargetHandle *pSceneTarget, graph::TextureHandle *pTexture);

        void create(RenderContext *ctx) override;
        void destroy(RenderContext *ctx) override;
        void execute(RenderContext *ctx) override;

        void updateUniform(RenderContext *ctx);

        PassResource<graph::SceneTargetHandle> *pSceneTarget;
        PassResource<graph::TextureHandle> *pTextureHandle;

        simcoe::Timer *pTimer;

        render::Display display;

        render::PipelineState *pPipeline;

        render::VertexBuffer *pQuadVertexBuffer;
        render::IndexBuffer *pQuadIndexBuffer;

        render::UniformBuffer *pQuadUniformBuffer;
        DataAlloc::Index quadUniformIndex;
    };
}
