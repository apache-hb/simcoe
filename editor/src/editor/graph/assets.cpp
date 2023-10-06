#include "editor/graph/assets.h"

using namespace editor;
using namespace editor::graph;

void SwapChainHandle::create() {
    const auto &createInfo = ctx->getCreateInfo();
    targets.resize(createInfo.backBufferCount);

    for (UINT i = 0; i < createInfo.backBufferCount; i++) {
        rhi::RenderTarget *pTarget = ctx->getRenderTarget(i);
        RenderTargetAlloc::Index rtvIndex = ctx->mapRenderTarget(pTarget);

        pTarget->setName("swapchain-target-" + std::to_string(i));

        targets[i] = { pTarget, rtvIndex, rhi::ResourceState::ePresent };
    }
}

void SwapChainHandle::destroy() {
    const auto& createInfo = ctx->getCreateInfo();
    auto *pRtvHeap = ctx->getRtvHeap();
    for (UINT i = 0; i < createInfo.backBufferCount; ++i) {
        const auto& frame = targets[i];
        delete frame.pRenderTarget;
        pRtvHeap->release(frame.rtvIndex);
    }

    targets.clear();
}

rhi::ResourceState SwapChainHandle::getCurrentState() const {
    return targets[ctx->getFrameIndex()].state;
}

void SwapChainHandle::setCurrentState(rhi::ResourceState state) {
    targets[ctx->getFrameIndex()].state = state;
}

rhi::DeviceResource *SwapChainHandle::getResource() const {
    return targets[ctx->getFrameIndex()].pRenderTarget;
}

RenderTargetAlloc::Index SwapChainHandle::getRtvIndex() const {
    return targets[ctx->getFrameIndex()].rtvIndex;
}


///
/// scene target handle
///

void SceneTargetHandle::create() {
    const auto& createInfo = ctx->getCreateInfo();

    const rhi::TextureInfo textureCreateInfo = {
        .width = createInfo.renderWidth,
        .height = createInfo.renderHeight,
        .format = rhi::PixelFormat::eRGBA8,
    };

    auto *pResource = ctx->createTextureRenderTarget(textureCreateInfo, kClearColour);
    pResource->setName("scene-target");

    setResource(pResource);
    setCurrentState(rhi::ResourceState::eShaderResource);
    setSrvIndex(ctx->mapTexture(pResource));
    setRtvIndex(ctx->mapRenderTarget(pResource));
}

void SceneTargetHandle::destroy() {
    auto *pRtvHeap = ctx->getRtvHeap();
    auto *pSrvHeap = ctx->getSrvHeap();
    pRtvHeap->release(getRtvIndex());
    pSrvHeap->release(getSrvIndex());
    delete getResource();
}

///
/// texture handle
///

TextureHandle::TextureHandle(Context *ctx, std::string name)
    : ISingleResourceHandle(ctx, name)
    , name(name)
{ }

void TextureHandle::create() {
    const auto& createInfo = ctx->getCreateInfo();
    assets::Image image = createInfo.depot.loadImage(name);

    const rhi::TextureInfo textureInfo = {
        .width = image.width,
        .height = image.height,

        .format = rhi::PixelFormat::eRGBA8
    };

    auto *pResource = ctx->createTexture(textureInfo);

    setResource(pResource);
    setSrvIndex(ctx->mapTexture(pResource));
    setCurrentState(rhi::ResourceState::eCopyDest);

    std::unique_ptr<rhi::UploadBuffer> pTextureStaging{ctx->createTextureUploadBuffer(textureInfo)};

    pResource->setName(name);
    pResource->setName("staging(" + name + ")");

    ctx->beginCopy();

    ctx->copyTexture(pResource, pTextureStaging.get(), textureInfo, image.data);

    ctx->endCopy();
}

void TextureHandle::destroy() {
    auto *pSrvHeap = ctx->getSrvHeap();
    pSrvHeap->release(getSrvIndex());
    delete getResource();
}
