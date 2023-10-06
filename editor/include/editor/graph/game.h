#pragma once

#include "editor/graph/assets.h"

#include "editor/game/state.h"

namespace editor::graph {
    struct UNIFORM_ALIGN CameraUniform {
        math::float4x4 model;
        math::float4x4 view;
        math::float4x4 projection;
    };

    struct UNIFORM_ALIGN ObjectUniform {
        math::float4x4 model;
    };

    struct CameraUniformHandle final : IUniformHandle<CameraUniform> {
        CameraUniformHandle(Context *ctx)
            : IUniformHandle(ctx, "uniform.camera", eDepRenderSize)
        { }

        void update(GameLevel *pLevel);
    };

    struct ObjectUniformHandle final : IUniformHandle<ObjectUniform> {
        ObjectUniformHandle(Context *ctx)
            : IUniformHandle(ctx, "uniform.object")
        { }

        void update(GameLevel *pLevel);
    };

    struct GameRenderInfo {
        ResourceWrapper<TextureHandle> *pPlayerTexture;
        ResourceWrapper<CameraUniformHandle> *pCameraUniform;
        ResourceWrapper<ObjectUniformHandle> *pPlayerUniform;
        IMeshBufferHandle *pPlayerMesh;
    };

    struct GameLevelPass final : IRenderPass {
        GameLevelPass(
            Context *ctx,
            GameLevel *pLevel,
            ResourceWrapper<IRTVHandle> *pRenderTarget,
            GameRenderInfo info
        );

        void create() override;
        void destroy() override;

        void execute() override;

    private:
        PassAttachment<IRTVHandle> *pRenderTarget;
        PassAttachment<TextureHandle> *pPlayerTexture;
        PassAttachment<CameraUniformHandle> *pCameraUniform;
        PassAttachment<ObjectUniformHandle> *pPlayerUniform;

        IMeshBufferHandle *pPlayerMesh;

        rhi::PipelineState *pPipeline;

        GameLevel *pLevel;
    };
}
