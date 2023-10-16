#pragma once

#include "editor/graph/assets.h"

#include "editor/game/level.h"

namespace editor::graph {
    struct UNIFORM_BUFFER CameraUniform {
        math::float4x4 view;
        math::float4x4 projection;
    };

    struct UNIFORM_BUFFER ObjectUniform {
        math::float4x4 model;
    };

    struct CameraUniformHandle final : IUniformHandle<CameraUniform> {
        CameraUniformHandle(Graph *ctx)
            : IUniformHandle(ctx, "uniform.camera", eDepRenderSize)
        { }

        void update(GameLevel *pLevel);
    };

    struct ObjectUniformHandle final : IUniformHandle<ObjectUniform> {
        ObjectUniformHandle(Graph *ctx, std::string_view name)
            : IUniformHandle(ctx, std::format("uniform.object.{}", name))
        { }

        void update(IGameObject *pObject);
    };

    struct GameRenderInfo {
        ResourceWrapper<CameraUniformHandle> *pCameraUniform;
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

        size_t addTexture(ResourceWrapper<TextureHandle> *pTexture);

    private:
        using TextureAttachment = PassAttachment<TextureHandle>;
        using ObjectAttachment = PassAttachment<ObjectUniformHandle>;

        PassAttachment<CameraUniformHandle> *pCameraUniform;

        std::vector<TextureAttachment*> textureAttachments;

        std::unordered_map<IGameObject*, ObjectAttachment*> objectUniforms;
        ObjectUniformHandle *getObjectUniform(IGameObject *pObject);
        void createObjectUniform(IGameObject *pObject);

        rhi::PipelineState *pPipeline;

        GameLevel *pLevel;
    };
}
