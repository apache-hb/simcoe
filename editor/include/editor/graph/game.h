#pragma once

#include "editor/graph/assets.h"

#include "editor/game/state.h"

namespace editor::graph {
    struct UNIFORM_ALIGN GameUniformData {
        math::float2 offset;
        float angle;
        float scale;

        float aspect;
    };

    struct GameUniformHandle final : IUniformHandle<GameUniformData> {
        GameUniformHandle(Context *ctx)
            : IUniformHandle(ctx, "uniform", eDepRenderSize)
        { }

        void update(GameLevel *pLevel);
    };

    struct GameRenderInfo {
        ResourceWrapper<TextureHandle> *pPlayerTexture;
        ResourceWrapper<GameUniformHandle> *pPlayerUniform;
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
        PassAttachment<GameUniformHandle> *pPlayerUniform;

        rhi::PipelineState *pPipeline;

        rhi::VertexBuffer *pQuadVertexBuffer;
        rhi::IndexBuffer *pQuadIndexBuffer;

        GameLevel *pLevel;
    };
}
