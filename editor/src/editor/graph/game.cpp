#include "editor/graph/game.h"

#include <array>

using namespace editor;
using namespace editor::graph;
using namespace editor::game;

using namespace simcoe::math;

constexpr auto kUpVector = float3::from(0.f, 0.f, 1.f); // z-up

void CameraUniformHandle::update(GameLevel *pLevel) {
    const auto& createInfo = ctx->getCreateInfo();
    float width = float(createInfo.renderWidth);
    float height = float(createInfo.renderHeight);

    float aspectRatio = width / height;

    float4x4 view = float4x4::lookToRH(pLevel->cameraPosition, pLevel->cameraRotation, kUpVector).transpose();
    float4x4 projection = pLevel->pProjection->getProjectionMatrix(aspectRatio).transpose();

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
    , pCameraUniform(addAttachment(info.pCameraUniform, rhi::ResourceState::eUniform))
    , pLevel(pLevel)
{
    setRenderTargetHandle(pRenderTarget);
    setDepthStencilHandle(pDepthTarget);
}

void GameLevelPass::create() {
    const auto &createInfo = ctx->getCreateInfo();
    // create pipeline
    const rhi::GraphicsPipelineInfo psoCreateInfo = {
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

    pPipeline = ctx->createGraphicsPipeline(psoCreateInfo);
    pPipeline->setName("pso.game");
}

void GameLevelPass::destroy() {
    delete pPipeline;
}

void GameLevelPass::execute() {
    CameraUniformHandle *pCamera = pCameraUniform->getInner();

    pCamera->update(pLevel);

    ctx->setGraphicsPipeline(pPipeline);

    UINT texIndex = pPipeline->getTextureInput("tex");
    UINT cameraIndex = pPipeline->getUniformInput("camera");
    UINT objectIndex = pPipeline->getUniformInput("object");

    ctx->setGraphicsShaderInput(cameraIndex, pCamera->getSrvIndex()); // set camera uniform

    std::unordered_set<IGameObject*> usedHandles;

    pLevel->useEachObject([&](IGameObject *pObject) {
        auto *pTextureHandle = textureAttachments[pObject->getTexture()];
        auto *pTexture = pTextureHandle->getInner();

        auto *pMesh = pObject->getMesh();

        auto *pUniform = getObjectUniform(pObject); // get object uniform (create if not exists)
        pUniform->update(pObject);

        usedHandles.emplace(pObject);

        ctx->setGraphicsShaderInput(texIndex, pTexture->getSrvIndex()); // set texture
        ctx->setGraphicsShaderInput(objectIndex, pUniform->getSrvIndex()); // set object uniform

        ctx->setVertexBuffer(pMesh->getVertexBuffer());
        ctx->setIndexBuffer(pMesh->getIndexBuffer());
        ctx->drawIndexed(pMesh->getIndexCount());
    });

    // remove unused object uniforms
    for (auto it = objectUniforms.begin(); it != objectUniforms.end();) {
        auto& [pObject, pAttachment] = *it;
        if (!usedHandles.contains(it->first)) {
            graph->removeResource(pAttachment->getResourceHandle());
            std::erase(inputs, pAttachment);
            it = objectUniforms.erase(it);
        } else {
            ++it;
        }
    }
}

size_t GameLevelPass::addTexture(ResourceWrapper<TextureHandle> *pTexture) {
    auto *pAttachment = addAttachment(pTexture, rhi::ResourceState::eTextureRead);
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
    objectUniforms.emplace(pObject, addAttachment(pUniform, rhi::ResourceState::eUniform));
}
