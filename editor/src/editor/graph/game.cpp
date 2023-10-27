#include "editor/graph/game.h"

#include "game/game.h"

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

void ObjectUniformHandle::update(IEntity *pObject) {
    float4x4 model = float4x4::identity();
    model *= float4x4::translation(pObject->position);
    model *= float4x4::rotation(pObject->rotation);
    model *= float4x4::scale(pObject->scale);

    ObjectUniform data = { .model = model };

    IUniformHandle::update(&data);
}

GameLevelPass::GameLevelPass(Graph *ctx, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<IDSVHandle> *pDepthTarget)
    : IRenderPass(ctx, "game.level")
{
    setRenderTargetHandle(pRenderTarget);
    setDepthStencilHandle(pDepthTarget);

    pCameraBuffer = graph->addResource<CameraUniformHandle>();
    pCameraAttachment = addAttachment(pCameraBuffer, rhi::ResourceState::eUniform);
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
    game::GameLevel *pLevel = game::getInstance()->getActiveLevel();
    if (pLevel == nullptr) return;

    CameraUniformHandle *pCamera = pCameraAttachment->getInner();

    pCamera->update(pLevel);

    ctx->setGraphicsPipeline(pPipeline);

    UINT texIndex = pPipeline->getTextureInput("tex");
    UINT cameraIndex = pPipeline->getUniformInput("camera");
    UINT objectIndex = pPipeline->getUniformInput("object");

    ctx->setGraphicsShaderInput(cameraIndex, pCamera->getSrvIndex()); // set camera uniform

    std::unordered_set<IEntity*> usedHandles;

    pLevel->useEachObject([&](IEntity *pObject) {
        auto *pTexture = getObjectTexture(pObject);
        auto *pUniform = getObjectUniform(pObject); // get object uniform (create if not exists)
        auto *pMesh = pObject->getMesh();

        pUniform->update(pObject);

        usedHandles.emplace(pObject);

        ctx->setGraphicsShaderInput(texIndex, pTexture->getSrvIndex()); // set texture
        ctx->setGraphicsShaderInput(objectIndex, pUniform->getSrvIndex()); // set object uniform

        ctx->setVertexBuffer(pMesh->getVertexBuffer());
        ctx->setIndexBuffer(pMesh->getIndexBuffer());
        ctx->drawIndexed(pMesh->getIndexCount());
    });

    // TODO: this causes a TDR :)
    // // remove unused object uniforms
    // for (auto it = objectUniforms.begin(); it != objectUniforms.end();) {
    //     auto& [pObject, pAttachment] = *it;
    //     if (!usedHandles.contains(it->first)) {
    //         graph->removeResource(pAttachment->getResourceHandle());
    //         std::erase(inputs, pAttachment);
    //         it = objectUniforms.erase(it);
    //     } else {
    //         ++it;
    //     }
    // }

    // // remove unused object textures
    // for (auto it = objectTextures.begin(); it != objectTextures.end();) {
    //     auto& [pObject, pAttachment] = *it;
    //     if (!usedHandles.contains(it->first)) {
    //         graph->removeResource(pAttachment->getResourceHandle());
    //         std::erase(inputs, pAttachment);
    //         it = objectTextures.erase(it);
    //     } else {
    //         ++it;
    //     }
    // }
}

///
/// uniform handling
///

ObjectUniformHandle *GameLevelPass::getObjectUniform(IEntity *pObject) {
    if (!objectUniforms.contains(pObject)) {
        createObjectUniform(pObject);
    }

    auto *pAttachment = objectUniforms.at(pObject);
    return pAttachment->getInner();
}

void GameLevelPass::createObjectUniform(IEntity *pObject) {
    auto *pUniform = graph->addResource<ObjectUniformHandle>(pObject->getName());
    objectUniforms.emplace(pObject, addAttachment(pUniform, rhi::ResourceState::eUniform));
}

///
/// texture handling
///

TextureHandle *GameLevelPass::getObjectTexture(game::IEntity *pObject) {
    ResourceWrapper<graph::TextureHandle> *pTextureHandle = pObject->getTexture();
    if (!objectTextures.contains(pTextureHandle->getInner())) {
        createObjectTexture(pTextureHandle);
    }

    auto *pAttachment = objectTextures.at(pTextureHandle->getInner());
    return pAttachment->getInner();
}

void GameLevelPass::createObjectTexture(ResourceWrapper<graph::TextureHandle> *pTexture) {
    auto *pAttachment = addAttachment(pTexture, rhi::ResourceState::eTextureRead);
    objectTextures.emplace(pTexture->getInner(), pAttachment);
}
