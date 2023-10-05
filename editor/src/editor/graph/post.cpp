#pragma once

#include "editor/graph/post.h"

#include <array>

using namespace editor;
using namespace editor::graph;

static constexpr math::float4 kBlackClearColour = { 0.0f, 0.0f, 0.0f, 1.0f };

static constexpr auto kScreenQuad = std::to_array<Vertex>({
    Vertex{ { 1.f, -1.f, 0.0f }, { 0.f, 0.f } }, // top left
    Vertex{ { 1.f, 1.f, 0.0f }, { 1.f, 0.f } }, // top right
    Vertex{ { -1.f, -1.f, 0.0f }, { 0.f, 1.f } }, // bottom left
    Vertex{ { -1.f, 1.f, 0.0f }, { 1.f, 1.f } } // bottom right
});

static constexpr auto kScreenQuadIndices = std::to_array<uint16_t>({
    0, 2, 1,
    1, 2, 3
});

static constexpr rhi::Display createLetterBoxDisplay(UINT renderWidth, UINT renderHeight, UINT displayWidth, UINT displayHeight) {
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

PostPass::PostPass(RenderContext *ctx, graph::SceneTargetHandle *pSceneTarget, graph::SwapChainHandle *pBackBuffers)
    : IRenderPass(ctx, "post", StateDep(eDepDisplaySize | eDepRenderSize))
    , pSceneTarget(addAttachment<graph::SceneTargetHandle>(pSceneTarget, rhi::ResourceState::eShaderResource))
    , pBackBuffers(addAttachment<graph::SwapChainHandle>(pBackBuffers, rhi::ResourceState::eRenderTarget))
{ }

void PostPass::create() {
    const auto& createInfo = ctx->getCreateInfo();

    display = createLetterBoxDisplay(createInfo.renderWidth, createInfo.renderHeight, createInfo.displayWidth, createInfo.displayHeight);

    const rhi::PipelineCreateInfo psoCreateInfo = {
        .vertexShader = createInfo.depot.loadBlob("blit.vs.cso"),
        .pixelShader = createInfo.depot.loadBlob("blit.ps.cso"),

        .attributes = {
            { "POSITION", offsetof(Vertex, position), rhi::TypeFormat::eFloat3 },
            { "TEXCOORD", offsetof(Vertex, uv), rhi::TypeFormat::eFloat2 }
        },

        .textureInputs = {
            { rhi::InputVisibility::ePixel, 0, false },
        },

        .samplers = {
            { rhi::InputVisibility::ePixel, 0 }
        }
    };

    pPipeline = ctx->createPipelineState(psoCreateInfo);

    std::unique_ptr<rhi::UploadBuffer> pVertexStaging{ctx->createUploadBuffer(kScreenQuad.data(), kScreenQuad.size() * sizeof(Vertex))};
    std::unique_ptr<rhi::UploadBuffer> pIndexStaging{ctx->createUploadBuffer(kScreenQuadIndices.data(), kScreenQuadIndices.size() * sizeof(uint16_t))};

    pScreenQuadVerts = ctx->createVertexBuffer(kScreenQuad.size(), sizeof(Vertex));
    pScreenQuadIndices = ctx->createIndexBuffer(kScreenQuadIndices.size(), rhi::TypeFormat::eUint16);

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
    IResourceHandle *pTarget = pSceneTarget->getHandle();
    IResourceHandle *pRenderTarget = pBackBuffers->getHandle();

    ctx->setPipeline(pPipeline);
    ctx->setDisplay(display);

    ctx->setRenderTarget(pRenderTarget->getRtvIndex(), kBlackClearColour);

    ctx->setShaderInput(pTarget->getSrvIndex(), 0);
    ctx->setVertexBuffer(pScreenQuadVerts);
    ctx->drawIndexBuffer(pScreenQuadIndices, kScreenQuadIndices.size());
}

///
/// present pass
///

PresentPass::PresentPass(RenderContext *ctx, graph::SwapChainHandle *pBackBuffers)
    : IRenderPass(ctx, "present")
    , pBackBuffers(addAttachment<graph::SwapChainHandle>(pBackBuffers, rhi::ResourceState::ePresent))
{ }

void PresentPass::create() {

}

void PresentPass::destroy() {

}

void PresentPass::execute() {

}

