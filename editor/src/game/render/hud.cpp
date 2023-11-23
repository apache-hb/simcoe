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

    if (auto *pBuffer = asResource()) {
        pBuffer->write(data.data(), data.size() * sizeof(UiIndex));
    }
}

void UiIndexBufferHandle::create() {
    auto *pDevice = ctx->getDevice();
    auto *pBuffer = pDevice->createIndexBuffer(size, rhi::TypeFormat::eUint16, rhi::HeapType::eUpload);
    setResource(pBuffer);
    setCurrentState(rhi::ResourceState::eIndexBuffer);
}

// vertex buffer

UiVertexBufferHandle::UiVertexBufferHandle(Graph *pGraph, size_t size)
    : Super(pGraph, "ui.vbo")
    , size(size)
{ }

void UiVertexBufferHandle::create() {
    auto *pDevice = ctx->getDevice();
    auto *pBuffer = pDevice->createVertexBuffer(size, sizeof(UiVertex), rhi::HeapType::eUpload);
    setResource(pBuffer);
    setCurrentState(rhi::ResourceState::eVertexBuffer);
}

void UiVertexBufferHandle::write(const std::vector<UiVertex>& vertices) {
    data = vertices;

    if (auto *pBuffer = asResource()) {
        pBuffer->write(data.data(), data.size() * sizeof(UiVertex));
    }
}

// render pass

HudPass::HudPass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget)
    : IRenderPass(pGraph, "game.hud")
{
    setRenderTargetHandle(pRenderTarget);

    pVertexBuffer = pGraph->addResource<UiVertexBufferHandle>(0x1000);
    pIndexBuffer = pGraph->addResource<UiIndexBufferHandle>(0x1000);

    pMatrix = pGraph->addResource<ModelUniform>("hud.matrix");

    utf8::StaticText alnum = u8" :!.+-,/*#abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    utf8::StaticText symbols = u8"" SM_XB_LOGO SM_XB_VIEW SM_XB_MENU SM_PLAYER_ICON SM_NAME_UNDERLINE;

    FontAtlasInfo text = {
        .path = "C:/Windows/Fonts/8514oem.fon",
        .pt = 32,
        .glyphs = std::vector<char32_t>(alnum.begin(), alnum.end())
    };

    FontAtlasInfo icons = {
        .path = "$vfs/swarm.ttf",
        .pt = 16,
        .glyphs = std::vector<char32_t>(symbols.begin(), symbols.end())
    };

    std::vector<FontAtlasInfo> fonts = { text, icons };

    pFontAtlas = pGraph->addResource<FontAtlasHandle>(fonts);

    addAttachment(pMatrix, rhi::ResourceState::eUniform);
    addAttachment(pVertexBuffer, rhi::ResourceState::eVertexBuffer);
    addAttachment(pIndexBuffer, rhi::ResourceState::eIndexBuffer);
    addAttachment(pFontAtlas, rhi::ResourceState::eTextureRead);
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
            { "atlas", rhi::InputVisibility::ePixel, 0, true }
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

    const auto& createInfo = ctx->getCreateInfo();

    float4x4 p = float4x4::orthographicRH(
        0.f, float(createInfo.renderWidth),
        0.f, -float(createInfo.renderHeight),
        0.0f, 1.0f
    );

    Model model = { p };

    pMatrix->getInner()->update(&model);
}

void HudPass::destroy() {
    delete pPipeline;
}

void HudPass::execute() {
    if (bDirty && lock.try_lock()) {
        pVertexBuffer->getInner()->write(vertices);
        pIndexBuffer->getInner()->write(indices);
        bDirty = false;
        lock.unlock();
    }
    
    ctx->setGraphicsPipeline(pPipeline);
    ctx->setGraphicsShaderInput(pPipeline->getTextureInput("atlas"), pFontAtlas->getInner()->getSrvIndex());
    ctx->setGraphicsShaderInput(pPipeline->getUniformInput("matrix"), pMatrix->getInner()->getSrvIndex());

    ctx->setVertexBuffer(pVertexBuffer->getInner()->asResource());
    ctx->setIndexBuffer(pIndexBuffer->getInner()->asResource());

    ctx->drawIndexed(indices.size());
}

void HudPass::update(const ui::Context& layout) {
    std::lock_guard guard(lock);
    vertices = layout.vertices;
    indices = layout.indices;
    bDirty = true;
}
