#include "editor/graph/game.h"

#include <array>

using namespace editor;
using namespace editor::graph;

static constexpr auto kQuadVerts = std::to_array<Vertex>({
    Vertex{ { 0.1f, -0.1f, 0.0f }, { 0.f, 0.f } }, // top left
    Vertex{ { 0.1f, 0.1f, 0.0f }, { 1.f, 0.f } }, // top right
    Vertex{ { -0.1f, -0.1f, 0.0f }, { 0.f, 1.f } }, // bottom left
    Vertex{ { -0.1f, 0.1f, 0.0f }, { 1.f, 1.f } } // bottom right
});

static constexpr auto kQuadIndices = std::to_array<uint16_t>({
    0, 2, 1,
    1, 2, 3
});

void GameUniformHandle::update(GameLevel *pLevel) {
    const auto& createInfo = ctx->getCreateInfo();
    float aspect = float(createInfo.renderHeight) / float(createInfo.renderWidth);

    GameUniformData data = {
        .offset = pLevel->playerOffset,
        .angle = pLevel->playerAngle,
        .scale = pLevel->playerScale,
        .aspect = aspect
    };

    IUniformHandle::update(&data);
}

GameLevelPass::GameLevelPass(Context *ctx, GameLevel *pLevel, ResourceWrapper<IRTVHandle> *pRenderTarget, GameRenderInfo info)
    : IRenderPass(ctx, "game.2d")
    , pRenderTarget(addAttachment(pRenderTarget, rhi::ResourceState::eRenderTarget))
    , pPlayerTexture(addAttachment(info.pPlayerTexture, rhi::ResourceState::eShaderResource))
    , pPlayerUniform(addAttachment(info.pPlayerUniform, rhi::ResourceState::eShaderResource))
    , pLevel(pLevel)
{ }

void GameLevelPass::create() {
    const auto &createInfo = ctx->getCreateInfo();
    // create pipeline
    const rhi::PipelineCreateInfo psoCreateInfo = {
        .vertexShader = createInfo.depot.loadBlob("game.vs.cso"),
        .pixelShader = createInfo.depot.loadBlob("game.ps.cso"),

        .attributes = {
            { "POSITION", offsetof(Vertex, position), rhi::TypeFormat::eFloat3 },
            { "TEXCOORD", offsetof(Vertex, uv), rhi::TypeFormat::eFloat2 }
        },

        .textureInputs = {
            { rhi::InputVisibility::ePixel, 0, true }
        },

        .uniformInputs = {
            { rhi::InputVisibility::eVertex, 0, false }
        },

        .samplers = {
            { rhi::InputVisibility::ePixel, 0 }
        }
    };

    pPipeline = ctx->createPipelineState(psoCreateInfo);

    // create vertex data
    std::unique_ptr<rhi::UploadBuffer> pVertexStaging{ctx->createUploadBuffer(kQuadVerts.data(), kQuadVerts.size() * sizeof(Vertex))};
    std::unique_ptr<rhi::UploadBuffer> pIndexStaging{ctx->createUploadBuffer(kQuadIndices.data(), kQuadIndices.size() * sizeof(uint16_t))};

    pQuadVertexBuffer = ctx->createVertexBuffer(kQuadVerts.size(), sizeof(Vertex));
    pQuadIndexBuffer = ctx->createIndexBuffer(kQuadIndices.size(), rhi::TypeFormat::eUint16);

    ctx->beginCopy();
    ctx->copyBuffer(pQuadVertexBuffer, pVertexStaging.get());
    ctx->copyBuffer(pQuadIndexBuffer, pIndexStaging.get());
    ctx->endCopy();
}

void GameLevelPass::destroy() {
    delete pPipeline;

    delete pQuadVertexBuffer;
    delete pQuadIndexBuffer;
}

void GameLevelPass::execute() {
    IRTVHandle *pTarget = pRenderTarget->getInner();
    ISRVHandle *pTexture = pPlayerTexture->getInner();
    GameUniformHandle *pUniform = pPlayerUniform->getInner();

    ctx->setRenderTarget(pTarget->getRtvIndex());

    ctx->setPipeline(pPipeline);

    ctx->setShaderInput(pTexture->getSrvIndex(), 0);
    ctx->setShaderInput(pUniform->getSrvIndex(), 1);

    pUniform->update(pLevel);

    ctx->setVertexBuffer(pQuadVertexBuffer);
    ctx->drawIndexBuffer(pQuadIndexBuffer, kQuadIndices.size());
}
