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

    float aspectRatio = width / height;

    float4x4 view = float4x4::lookToRH(pLevel->cameraPosition, pLevel->cameraRotation, kUpVector).transpose();
    //float4x4 projection = float4x4::perspectiveRH(pLevel->fov * kDegToRad<float>, width / height, 0.1f, 1000.f).transpose();
    float4x4 projection = pLevel->pProjection->getProjectionMatrix(aspectRatio, pLevel->fov).transpose(); // float4x4::orthographicRH(20 * aspectRatio, 20, 0.1f, 125.f).transpose();

    CameraUniform data = {
        .view = view,
        .projection = projection
    };

    IUniformHandle::update(&data);
}

void ObjectUniformHandle::update(IGameObject *pObject) {
    float4x4 model = float4x4::identity();
    model *= float4x4::translation(pObject->position);
    model *= float4x4::rotation(pObject->rotation);
    model *= float4x4::scaling(pObject->scale);

    ObjectUniform data = { .model = model };

    IUniformHandle::update(&data);
}

GameLevelPass::GameLevelPass(Graph *ctx, GameLevel *pLevel, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<IDSVHandle> *pDepthTarget, GameRenderInfo info)
    : IRenderPass(ctx, "game.level")
    , pCameraUniform(addAttachment(info.pCameraUniform, rhi::ResourceState::eShaderResource))
    , pLevel(pLevel)
{
    setRenderTargetHandle(pRenderTarget);
    setDepthStencilHandle(pDepthTarget);

    pLevel->useEachObject([this](IGameObject *pObject) {
        createObjectUniform(pObject);
    });
}

void GameLevelPass::create() {
    const auto &createInfo = ctx->getCreateInfo();
    // create pipeline
    const rhi::PipelineCreateInfo psoCreateInfo = {
        .vertexShader = createInfo.depot.loadBlob("object.vs.cso"),
        .pixelShader = createInfo.depot.loadBlob("object.ps.cso"),

        .attributes = {
            { "POSITION", offsetof(ObjVertex, position), rhi::TypeFormat::eFloat3 },
            { "TEXCOORD", offsetof(ObjVertex, uv), rhi::TypeFormat::eFloat2 }
        },

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
    CameraUniformHandle *pCamera = pCameraUniform->getInner();

    pCamera->update(pLevel);

    ctx->setPipeline(pPipeline);

    UINT texIndex = pPipeline->getTextureInput("tex");
    UINT cameraIndex = pPipeline->getUniformInput("camera");
    UINT objectIndex = pPipeline->getUniformInput("object");

    ctx->setShaderInput(pCamera->getSrvIndex(), cameraIndex); // set camera uniform

    pLevel->useEachObject([&](IGameObject *pObject) {
        auto *pTextureHandle = textureAttachments[pObject->getTexture()];
        auto *pTexture = pTextureHandle->getInner();

        auto *pMesh = pObject->getMesh();

        auto *pUniform = getObjectUniform(pObject); // get object uniform (create if not exists)
        pUniform->update(pObject);

        ctx->setShaderInput(pTexture->getSrvIndex(), texIndex); // set texture
        ctx->setShaderInput(pUniform->getSrvIndex(), objectIndex); // set object uniform

        ctx->setVertexBuffer(pMesh->getVertexBuffer());
        ctx->setIndexBuffer(pMesh->getIndexBuffer());
        ctx->drawIndexed(pMesh->getIndexCount());
    });
}

size_t GameLevelPass::addTexture(ResourceWrapper<TextureHandle> *pTexture) {
    auto *pAttachment = addAttachment(pTexture, rhi::ResourceState::eShaderResource);
    textureAttachments.push_back(pAttachment);
    return textureAttachments.size() - 1;
}

ObjectUniformHandle *GameLevelPass::getObjectUniform(IGameObject *pObject) {
    if (!objectUniforms.contains(pObject)) {
        createObjectUniform(pObject);
    }

    auto *pAttachment = objectUniforms.at(pObject);
    return pAttachment->getInner();
}

void GameLevelPass::createObjectUniform(IGameObject *pObject) {
    auto *pUniform = graph->addResource<ObjectUniformHandle>(pObject->getName());
    objectUniforms.emplace(pObject, addAttachment(pUniform, rhi::ResourceState::eShaderResource));
}
