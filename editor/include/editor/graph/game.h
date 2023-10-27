#pragma once

#include "editor/graph/assets.h"

#include "game/level.h"

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

        void update(game::GameLevel *pLevel);
    };

    struct ObjectUniformHandle final : IUniformHandle<ObjectUniform> {
        ObjectUniformHandle(Graph *ctx, std::string_view name)
            : IUniformHandle(ctx, std::format("uniform.object.{}", name))
        { }

        void update(game::IGameObject *pObject);
    };

    struct GameLevelPass final : IRenderPass {
        GameLevelPass(
            Graph *ctx,
            ResourceWrapper<IRTVHandle> *pRenderTarget,
            ResourceWrapper<IDSVHandle> *pDepthTarget
        );

        void create() override;
        void destroy() override;

        void execute() override;

    private:
        using TextureAttachment = PassAttachment<TextureHandle>;
        using ObjectAttachment = PassAttachment<ObjectUniformHandle>;

        ResourceWrapper<CameraUniformHandle> *pCameraBuffer;
        PassAttachment<CameraUniformHandle> *pCameraAttachment;

        std::unordered_map<game::IGameObject*, ObjectAttachment*> objectUniforms;
        ObjectUniformHandle *getObjectUniform(game::IGameObject *pObject);
        void createObjectUniform(game::IGameObject *pObject);

        std::unordered_map<ITextureHandle*, TextureAttachment*> objectTextures;
        TextureHandle *getObjectTexture(game::IGameObject *pObject);
        void createObjectTexture(ResourceWrapper<graph::TextureHandle> *pTexture);

        rhi::PipelineState *pPipeline;
    };
}
