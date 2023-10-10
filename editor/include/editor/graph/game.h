#pragma once

#include "editor/graph/assets.h"

#include "editor/game/state.h"

namespace editor::graph {
    struct UNIFORM_ALIGN CameraUniform {
        math::float4x4 view;
        math::float4x4 projection;
    };

    struct UNIFORM_ALIGN ObjectUniform {
        math::float4x4 model;
    };

    struct CameraUniformHandle final : IUniformHandle<CameraUniform> {
        CameraUniformHandle(Graph *ctx)
            : IUniformHandle(ctx, "uniform.camera", eDepRenderSize)
        { }

        void update(GameLevel *pLevel);
    };

    struct ObjectUniformHandle final : IUniformHandle<ObjectUniform> {
        ObjectUniformHandle(Graph *ctx, std::string name)
            : IUniformHandle(ctx, std::format("uniform.object.{}", name))
        { }

        void update(GameLevel *pLevel, size_t index);
    };

    struct GameRenderInfo {
        ResourceWrapper<TextureHandle> *pPlayerTexture;
        ResourceWrapper<CameraUniformHandle> *pCameraUniform;
        IMeshBufferHandle *pPlayerMesh;
    };

    struct GameLevelPass final : IRenderPass {
        GameLevelPass(
            Graph *ctx,
            GameLevel *pLevel,
            ResourceWrapper<IRTVHandle> *pRenderTarget,
            ResourceWrapper<IDSVHandle> *pDepthTarget,
            GameRenderInfo info
        );

        void create() override;
        void destroy() override;

        void execute() override;

    private:
        PassAttachment<IDSVHandle> *pDepthTarget;
        PassAttachment<TextureHandle> *pPlayerTexture;
        PassAttachment<CameraUniformHandle> *pCameraUniform;

        std::vector<PassAttachment<ObjectUniformHandle>*> objectUniforms;

        IMeshBufferHandle *pPlayerMesh;

        rhi::PipelineState *pPipeline;

        GameLevel *pLevel;
    };
}
