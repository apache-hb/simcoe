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
        .view = view,
        .projection = projection
    };

    IUniformHandle::update(&data);
}

void ObjectUniformHandle::update(GameLevel *pLevel, size_t index) {
    const auto &object = pLevel->objects[index];

    float4x4 model = float4x4::identity();
    model *= float4x4::translation(object.position);
    model *= float4x4::rotation(object.rotation);
    model *= float4x4::scaling(object.scale);

    ObjectUniform data = { .model = model };

    IUniformHandle::update(&data);
}

GameLevelPass::GameLevelPass(Graph *ctx, GameLevel *pLevel, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<IDSVHandle> *pDepthTarget, GameRenderInfo info)
    : IRenderPass(ctx, "game.level")
    , pDepthTarget(addAttachment(pDepthTarget, rhi::ResourceState::eDepthWrite))
    , pPlayerTexture(addAttachment(info.pPlayerTexture, rhi::ResourceState::eShaderResource))
    , pCameraUniform(addAttachment(info.pCameraUniform, rhi::ResourceState::eShaderResource))
    , pPlayerMesh(info.pPlayerMesh)
    , pLevel(pLevel)
{
    setRenderTargetHandle(pRenderTarget);
    setDepthStencilHandle(pDepthTarget);

    for (auto &object : pLevel->objects) {
        auto *pUniform = graph->addResource<ObjectUniformHandle>(object.name);
        objectUniforms.push_back(addAttachment(pUniform, rhi::ResourceState::eShaderResource));
    }
}

void GameLevelPass::create() {
    const auto &createInfo = ctx->getCreateInfo();
    // create pipeline
    const rhi::PipelineCreateInfo psoCreateInfo = {
        .vertexShader = createInfo.depot.loadBlob("object.vs.cso"),
        .pixelShader = createInfo.depot.loadBlob("object.ps.cso"),

        .attributes = pPlayerMesh->getVertexAttributes(),

        .textureInputs = {
            { "tex", rhi::InputVisibility::ePixel, 0, true }
        },

        .uniformInputs = {
            { "camera", rhi::InputVisibility::eVertex, 0, false },
            { "object", rhi::InputVisibility::eVertex, 1, false }
        },

        .samplers = {
            { rhi::InputVisibility::ePixel, 0 }
        },

        .rtvFormat = ctx->getSwapChainFormat(),
        .depthEnable = true,
        .dsvFormat = ctx->getDepthFormat()
    };

    pPipeline = ctx->createPipelineState(psoCreateInfo);
    pPipeline->setName("pso.game");
}

void GameLevelPass::destroy() {
    delete pPipeline;
}

void GameLevelPass::execute() {
    ISRVHandle *pTexture = pPlayerTexture->getInner();
    CameraUniformHandle *pCamera = pCameraUniform->getInner();

    pCamera->update(pLevel);

    ctx->setPipeline(pPipeline);

    UINT texIndex = pPipeline->getTextureInput("tex");
    UINT cameraIndex = pPipeline->getUniformInput("camera");
    UINT objectIndex = pPipeline->getUniformInput("object");

    ctx->setShaderInput(pTexture->getSrvIndex(), texIndex);
    ctx->setShaderInput(pCamera->getSrvIndex(), cameraIndex); // set camera uniform

    ctx->setVertexBuffer(pPlayerMesh->getVertexBuffer());
    ctx->setIndexBuffer(pPlayerMesh->getIndexBuffer());

    for (size_t i = 0; i < objectUniforms.size(); i++) {
        auto *pUniformHandle = objectUniforms[i];
        auto *pUniform = pUniformHandle->getInner();

        pUniform->update(pLevel, i);
        ctx->setShaderInput(pUniform->getSrvIndex(), objectIndex); // set object uniform
        ctx->drawIndexed(pPlayerMesh->getIndexCount());
    }
}
