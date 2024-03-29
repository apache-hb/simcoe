#include "editor/graph/post.h"

#include "engine/depot/service.h"

#include <array>

using namespace editor;
using namespace editor::graph;

using namespace simcoe;

static constexpr float quadX = 1.f;
static constexpr float quadY = 1.f;

static constexpr auto kScreenQuad = std::to_array<Vertex>({
    { { -quadX, quadY, 0.0f }, { 0.0f, 0.0f } },
    { { quadX, quadY, 0.0f }, { 1.0f, 0.0f } },
    { { quadX, -quadY, 0.0f }, { 1.0f, 1.0f } },
    { { -quadX, -quadY, 0.0f }, { 0.0f, 1.0f } }
});

static constexpr auto kScreenQuadIndices = std::to_array<uint16_t>({
    0, 1, 2,
    0, 2, 3
});

static rhi::Display createLetterBoxDisplay(UINT renderWidth, UINT renderHeight, UINT displayWidth, UINT displayHeight) {
    auto widthRatio = float(renderWidth) / displayWidth;
    auto heightRatio = float(renderHeight) / displayHeight;

    float x = 1.f;
    float y = 1.f;

    if (widthRatio < heightRatio) {
        x = widthRatio / heightRatio;
    } else {
        y = heightRatio / widthRatio;
    }

    rhi::Viewport viewport = {
        .x = displayWidth * (1.f - x) / 2.f,
        .y = displayHeight * (1.f - y) / 2.f,
        .width = x * displayWidth,
        .height = y * displayHeight,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    rhi::Scissor scissor = {
        LONG(viewport.x),
        LONG(viewport.y),
        LONG(viewport.x + viewport.width),
        LONG(viewport.y + viewport.height)
    };

    return { viewport, scissor };
}

PostPass::PostPass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<ISRVHandle> *pSceneSource)
    : IRenderPass(pGraph, "post", StateDep(eDepDisplaySize | eDepRenderSize))
    , pSceneSource(addAttachment(pSceneSource, rhi::ResourceState::eTextureRead))
{
    setRenderTargetHandle(pRenderTarget);
}

void PostPass::create() {
    const auto& createInfo = ctx->getCreateInfo();

    display = createLetterBoxDisplay(createInfo.renderWidth, createInfo.renderHeight, createInfo.displayWidth, createInfo.displayHeight);

    const rhi::GraphicsPipelineInfo psoCreateInfo = {
        .vertexShader = DepotService::openFile("blit.vs.cso")->blob(),
        .pixelShader = DepotService::openFile("blit.ps.cso")->blob(),

        .attributes = {
            { "POSITION", offsetof(Vertex, position), rhi::TypeFormat::eFloat3 },
            { "TEXCOORD", offsetof(Vertex, uv), rhi::TypeFormat::eFloat2 }
        },

        .textureInputs = {
            { "source", rhi::InputVisibility::ePixel, 0, false },
        },

        .samplers = {
            { rhi::InputVisibility::ePixel, 0 }
        },

        .rtvFormat = ctx->getSwapChainFormat()
    };

    pPipeline = ctx->createGraphicsPipeline(psoCreateInfo);
    pPipeline->setName("pso.post");

    std::unique_ptr<rhi::UploadBuffer> pVertexStaging{ctx->createUploadBuffer(kScreenQuad.data(), kScreenQuad.size() * sizeof(Vertex))};
    std::unique_ptr<rhi::UploadBuffer> pIndexStaging{ctx->createUploadBuffer(kScreenQuadIndices.data(), kScreenQuadIndices.size() * sizeof(uint16_t))};

    pScreenQuadVerts = ctx->createVertexBuffer(kScreenQuad.size(), sizeof(Vertex));
    pScreenQuadIndices = ctx->createIndexBuffer(kScreenQuadIndices.size(), rhi::TypeFormat::eUint16);

    pVertexStaging->setName("vbo-staging.screen");
    pIndexStaging->setName("ibo-staging.screen");

    pScreenQuadVerts->setName("vbo.screen");
    pScreenQuadIndices->setName("ibo.screen");

    ctx->beginCopy();

    ctx->copyBuffer(pScreenQuadVerts, pVertexStaging.get());
    ctx->copyBuffer(pScreenQuadIndices, pIndexStaging.get());

    ctx->endCopy();
}

void PostPass::destroy() {
    delete pPipeline;

    delete pScreenQuadVerts;
    delete pScreenQuadIndices;
}

void PostPass::execute() {
    ISRVHandle *pSource = pSceneSource->getInner();
    UINT srcSlot = pPipeline->getTextureInput("source");

    ctx->setGraphicsPipeline(pPipeline);
    ctx->setDisplay(display);

    ctx->setGraphicsShaderInput(srcSlot, pSource->getSrvIndex());
    ctx->setVertexBuffer(pScreenQuadVerts);
    ctx->drawIndexBuffer(pScreenQuadIndices, kScreenQuadIndices.size());
}

///
/// present pass
///

PresentPass::PresentPass(Graph *ctx, ResourceWrapper<SwapChainHandle> *pBackBuffers)
    : ICommandPass(ctx, "present")
    , pBackBuffers(addAttachment(pBackBuffers, rhi::ResourceState::ePresent))
{ }
