#include "editor/graph/mipmap.h"

using namespace editor;
using namespace editor::graph;

// resources

MipMapInfoHandle::MipMapInfoHandle(Graph *pGraph)
    : IUniformHandle(pGraph, "mipmap.info")
{ }

RwTextureHandle::RwTextureHandle(Graph *pGraph, uint2 size, size_t mipLevel)
    : ISingleResourceHandle(pGraph, "rwtexture", eDepDevice)
    , size(size)
    , mipLevel(mipLevel)
{ }

void RwTextureHandle::create() {
    const rhi::TextureInfo createInfo = {
        .width = size.x,
        .height = size.y,
        .format = rhi::TypeFormat::eRGBA8,
    };

    auto *pResource = ctx->createRwTexture(createInfo);
    pResource->setName("rwtexture");

    setResource(pResource);
    setCurrentState(rhi::ResourceState::eTextureWrite);
    setUavIndex(ctx->mapRwTexture(pResource, mipLevel));
}

void RwTextureHandle::destroy() {
    ISingleUAVHandle::destroy(ctx);
    ISingleResourceHandle::destroy();
}

// render pass

MipMapPass::MipMapPass(Graph *pGraph, ResourceWrapper<TextureHandle> *pSourceTexture, size_t mipLevels)
    : ICommandPass(pGraph, "mipmap")
    , pSourceTexture(addAttachment(pSourceTexture->as<ISRVHandle>(), rhi::ResourceState::eTextureRead))
    , mipLevels(mipLevels)
{
    TextureHandle *pTexture = pSourceTexture->getInner();
    pMipMapInfo = pGraph->addResource<MipMapInfoHandle>();
    pMipMapInfoAttachment = addAttachment(pMipMapInfo, rhi::ResourceState::eUniform);

    pMipMapTargets = std::make_unique<MipMapTarget[]>(mipLevels);
    uint2 size = pTexture->getSize();

    for (size_t i = 0; i < mipLevels; i++) {
        auto *pMipTexture = pGraph->addResource<RwTextureHandle>(size, i);
        auto *pMipAttachment = addAttachment(pMipTexture->as<IUAVHandle>(), rhi::ResourceState::eTextureWrite);

        size /= 2;

        pMipMapTargets[i] = { pMipTexture, pMipAttachment };
    }
}

void MipMapPass::create() {
    const auto& info = ctx->getCreateInfo();

    const rhi::ComputePipelineInfo createInfo = {
        .computeShader = info.depot.loadBlob("mipmap.cs.cso"),

        .textureInputs = {
            { "src", rhi::InputVisibility::eCompute, 0, true }
        },
        .uniformInputs = {
            { "info", rhi::InputVisibility::eCompute, 0, true }
        },
        .uavInputs = {
            { "dst", rhi::InputVisibility::eCompute, 0, false }
        },
        .samplers = {
            { rhi::InputVisibility::eCompute, 0 }
        }
    };

    pPipelineState = ctx->createComputePipeline(createInfo);
}

void MipMapPass::destroy() {
    delete pPipelineState;
}

void MipMapPass::execute() {
    ISRVHandle *pSource = pSourceTexture->getInner();
    ISRVHandle *pMipMapInfo = pMipMapInfoAttachment->getInner();

    ctx->setComputePipeline(pPipelineState);

    UINT srcSlot = pPipelineState->getTextureInput("src");
    UINT infoSlot = pPipelineState->getUniformInput("info");
    UINT dstSlot = pPipelineState->getUavInput("dst");

    ctx->setComputeShaderInput(srcSlot, pSource->getSrvIndex());
    ctx->setComputeShaderInput(infoSlot, pMipMapInfo->getSrvIndex());

    // TODO: this
    for (size_t i = 0; i < mipLevels; i++) {
        auto [pTarget, pTargetAttachment] = pMipMapTargets[i];

        auto *pTargetUav = pTargetAttachment->getInner();

        ctx->setComputeShaderInput(dstSlot, pTargetUav->getUavIndex());
        ctx->dispatchCompute(16, 16);
    }
}
