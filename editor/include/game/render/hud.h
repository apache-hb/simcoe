#pragma once

#include "engine/render/graph.h"

#include "editor/graph/assets.h"

#include "game/render/hud/layout.h"
#include "game/render/scene.h"

namespace game::render {
    struct UiVertexBufferHandle final : ISingleResourceHandle<rhi::VertexBuffer> {
        using Super = ISingleResourceHandle<rhi::VertexBuffer>;
        UiVertexBufferHandle(Graph *pGraph, size_t size);

        void write(const std::vector<ui::UiVertex>& vertices);

        void create() override;

    private:
        size_t size;
        std::vector<ui::UiVertex> data;
    };

    struct UiIndexBufferHandle final : ISingleResourceHandle<rhi::IndexBuffer> {
        using Super = ISingleResourceHandle<rhi::IndexBuffer>;
        UiIndexBufferHandle(Graph *pGraph, size_t size);

        void write(const std::vector<ui::UiIndex>& indices);

        void create() override;

    private:
        size_t size;
        std::vector<ui::UiIndex> data;
    };

    struct HudPass final : IRenderPass {
        HudPass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget);

        void create() override;
        void destroy() override;

        void execute() override;

    private:
        ResourceWrapper<UiVertexBufferHandle> *pVertexBuffer;
        ResourceWrapper<UiIndexBufferHandle> *pIndexBuffer;

        ResourceWrapper<ModelUniform> *pMatrix;

        rhi::PipelineState *pPipeline;
    };
}
