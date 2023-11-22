#include "game/render/hud.h"

#include "engine/depot/service.h"

using namespace game::render;
using namespace game::ui;

using simcoe::DepotService;

// index buffer

UiIndexBufferHandle::UiIndexBufferHandle(Graph *pGraph, size_t size)
    : Super(pGraph, "ui.ibo")
    , size(size)
{ }

void UiIndexBufferHandle::write(const std::vector<UiIndex>& indices) {
    data = indices;
}

void UiIndexBufferHandle::create() {
    auto *pBuffer = ctx->createIndexBuffer(size, rhi::TypeFormat::eUint16);
    setResource(pBuffer);
    setCurrentState(rhi::ResourceState::eIndexBuffer);
}

// vertex buffer

UiVertexBufferHandle::UiVertexBufferHandle(Graph *pGraph, size_t size)
    : Super(pGraph, "ui.vbo")
    , size(size)
{ }

void UiVertexBufferHandle::create() {
    auto *pBuffer = ctx->createVertexBuffer(size, sizeof(UiVertex));
    setResource(pBuffer);
    setCurrentState(rhi::ResourceState::eVertexBuffer);
}

void UiVertexBufferHandle::write(const std::vector<UiVertex>& vertices) {
    data = vertices;
}

// render pass

HudPass::HudPass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget)
    : IRenderPass(pGraph, "game.hud")
{
    setRenderTargetHandle(pRenderTarget);

    pVertexBuffer = pGraph->addResource<UiVertexBufferHandle>(0x1000);
    pIndexBuffer = pGraph->addResource<UiIndexBufferHandle>(0x1000);

    pMatrix = pGraph->addResource<ModelUniform>("hud.matrix");

    addAttachment(pMatrix, rhi::ResourceState::eUniform);
    addAttachment(pVertexBuffer, rhi::ResourceState::eVertexBuffer);
    addAttachment(pIndexBuffer, rhi::ResourceState::eIndexBuffer);
}

void HudPass::create() {
    const rhi::GraphicsPipelineInfo info = {
        .vertexShader = DepotService::openFile("hud.vs.cso")->blob(),
        .pixelShader = DepotService::openFile("hud.ps.cso")->blob(),

        .attributes = {
            { "POSITION", offsetof(UiVertex, position), rhi::TypeFormat::eFloat2 },
            { "TEXCOORD", offsetof(UiVertex, uv), rhi::TypeFormat::eFloat2 },
            { "COLOUR", offsetof(UiVertex, colour), rhi::TypeFormat::eRGBA8 }
        },

        .textureInputs = {
            { "text", rhi::InputVisibility::ePixel, 0, true }
        },

        .uniformInputs = {
            { "matrix", rhi::InputVisibility::eVertex, 0, false }
        },

        .samplers = {
            { rhi::InputVisibility::ePixel, 0 }
        },

        .rtvFormat = ctx->getSwapChainFormat()
    };

    pPipeline = ctx->createGraphicsPipeline(info);
    pPipeline->setName("pso.hud");
}

void HudPass::destroy() {
    delete pPipeline;
}

void HudPass::execute() {

}
