#pragma once

#include "editor/graph/assets.h"

#include "game/world.h"

namespace editor::graph {
    struct UNIFORM_BUFFER CameraUniform {
        math::float4x4 view;
        math::float4x4 projection;
    };

    struct UNIFORM_BUFFER ObjectUniform {
        math::float4x4 model;
    };

    struct CameraUniformHandle final : IUniformHandle<CameraUniform> {
        CameraUniformHandle(Graph *pGraph)
            : IUniformHandle(pGraph, "uniform.camera", eDepRenderSize)
        { }

        void update(game::World *pLevel);
    };

    struct ObjectUniformHandle final : IUniformHandle<ObjectUniform> {
        ObjectUniformHandle(Graph *pGraph, std::string_view name)
            : IUniformHandle(pGraph, std::format("uniform.object.{}", name))
        { }

        void update(game::IEntity *pObject);
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

        std::unordered_map<game::IEntity*, ObjectAttachment*> objectUniforms;
        ObjectUniformHandle *getObjectUniform(game::IEntity *pObject);
        void createObjectUniform(game::IEntity *pObject);

        std::unordered_map<ITextureHandle*, TextureAttachment*> objectTextures;
        TextureHandle *getObjectTexture(game::IEntity *pObject);
        void createObjectTexture(ResourceWrapper<graph::TextureHandle> *pTexture);

        rhi::PipelineState *pPipeline;
    };
}
