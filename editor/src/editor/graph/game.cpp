#include "editor/graph/game.h"

#include <array>

using namespace editor;
using namespace editor::graph;
using namespace simcoe::math;

constexpr auto kUpVector = float3::from(0.f, 0.f, 1.f); // z-up
constexpr auto kForwardVector = float3::from(1.f, 0.f, 0.f); // x-forward

void CameraUniformHandle::update(GameLevel *pLevel) {
    const auto& createInfo = ctx->getCreateInfo();
    float width = float(createInfo.renderWidth);
    float height = float(createInfo.renderHeight);

    float4x4 view = float4x4::lookToRH(pLevel->cameraPosition, kForwardVector, kUpVector).transpose();
    float4x4 projection = float4x4::perspectiveRH(pLevel->fov * kDegToRad<float>, width / height, 0.1f, 1000.f).transpose();

    CameraUniform data = {
        .model = float4x4::identity(),
        .view = view,
        .projection = projection
    };

    IUniformHandle::update(&data);
}

void ObjectUniformHandle::update(GameLevel *pLevel) {
    ObjectUniform data = { .model = float4x4::identity() };

    IUniformHandle::update(&data);
}

GameLevelPass::GameLevelPass(Graph *ctx, GameLevel *pLevel, ResourceWrapper<IRTVHandle> *pRenderTarget, GameRenderInfo info)
    : IRenderPass(ctx, "game.level")
    , pRenderTarget(addAttachment(pRenderTarget, rhi::ResourceState::eRenderTarget))
    , pPlayerTexture(addAttachment(info.pPlayerTexture, rhi::ResourceState::eShaderResource))
    , pCameraUniform(addAttachment(info.pCameraUniform, rhi::ResourceState::eShaderResource))
    , pPlayerUniform(addAttachment(info.pPlayerUniform, rhi::ResourceState::eShaderResource))
    , pPlayerMesh(info.pPlayerMesh)
    , pLevel(pLevel)
{ }

void GameLevelPass::create() {
    const auto &createInfo = ctx->getCreateInfo();
    // create pipeline
    const rhi::PipelineCreateInfo psoCreateInfo = {
        .vertexShader = createInfo.depot.loadBlob("object.vs.cso"),
        .pixelShader = createInfo.depot.loadBlob("object.ps.cso"),

        .attributes = pPlayerMesh->getVertexAttributes(),

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
}

void GameLevelPass::destroy() {
    delete pPipeline;
}

void GameLevelPass::execute() {
    IRTVHandle *pTarget = pRenderTarget->getInner();
    ISRVHandle *pTexture = pPlayerTexture->getInner();
    CameraUniformHandle *pCamera = pCameraUniform->getInner();
    ObjectUniformHandle *pUniform = pPlayerUniform->getInner();

    pCamera->update(pLevel);
    pUniform->update(pLevel);

    ctx->setRenderTarget(pTarget->getRtvIndex());

    ctx->setPipeline(pPipeline);

    ctx->setShaderInput(pTexture->getSrvIndex(), 0);
    ctx->setShaderInput(pCamera->getSrvIndex(), 1); // set camera uniform

    ctx->setVertexBuffer(pPlayerMesh->getVertexBuffer());
    ctx->drawIndexBuffer(pPlayerMesh->getIndexBuffer(), pPlayerMesh->getIndexCount());
}
