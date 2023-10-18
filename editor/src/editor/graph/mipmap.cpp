#include "editor/graph/mipmap.h"

using namespace editor;
using namespace editor::graph;

// resources

MipMapInfoHandle::MipMapInfoHandle(Graph *graph)
    : IUniformHandle(graph, "mipmap.info")
{ }

RwTextureHandle::RwTextureHandle(Graph *ctx, uint2 size, size_t mipLevel)
    : ISingleResourceHandle(ctx, "rwtexture", eDepDevice)
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

MipMapPass::MipMapPass(Graph *ctx, ResourceWrapper<TextureHandle> *pSourceTexture)
    : ICommandPass(ctx, "mipmap")
    , pSourceTexture(addAttachment(pSourceTexture->as<ISRVHandle>(), rhi::ResourceState::eTextureRead))
{
    TextureHandle *pTexture = pSourceTexture->getInner();
    pMipMapInfo = graph->addResource<MipMapInfoHandle>();
    pMipMapInfoAttachment = addAttachment(pMipMapInfo, rhi::ResourceState::eUniform);

    pTargetTexture = graph->addResource<RwTextureHandle>(pTexture->getSize(), 0);
    pTargetTextureAttachment = addAttachment(pTargetTexture->as<IUAVHandle>(), rhi::ResourceState::eTextureWrite);
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
    IUAVHandle *pTarget = pTargetTexture->getInner();

    ctx->setComputePipeline(pPipelineState);

    UINT srcSlot = pPipelineState->getTextureInput("src");
    UINT infoSlot = pPipelineState->getUniformInput("info");
    UINT dstSlot = pPipelineState->getUavInput("dst");

    ctx->setComputeShaderInput(srcSlot, pSource->getSrvIndex());
    ctx->setComputeShaderInput(infoSlot, pMipMapInfo->getSrvIndex());
    ctx->setComputeShaderInput(dstSlot, pTarget->getUavIndex());

    ctx->dispatchCompute(16, 16);
}
