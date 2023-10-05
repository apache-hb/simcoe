#include "editor/graph/assets.h"

using namespace editor;
using namespace editor::graph;

void SwapChainHandle::create() {
    const auto &createInfo = ctx->getCreateInfo();
    targets.resize(createInfo.backBufferCount);

    for (UINT i = 0; i < createInfo.backBufferCount; i++) {
        render::RenderTarget *pTarget = ctx->getRenderTarget(i);
        RenderTargetAlloc::Index rtvIndex = ctx->mapRenderTarget(pTarget);

        pTarget->setName("swapchain-target-" + std::to_string(i));

        targets[i] = { pTarget, rtvIndex, render::ResourceState::ePresent };
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

render::ResourceState SwapChainHandle::getCurrentState() const {
    return targets[ctx->getFrameIndex()].state;
}

void SwapChainHandle::setCurrentState(render::ResourceState state) {
    targets[ctx->getFrameIndex()].state = state;
}

render::DeviceResource *SwapChainHandle::getResource() const {
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

    const render::TextureInfo textureCreateInfo = {
        .width = createInfo.renderWidth,
        .height = createInfo.renderHeight,
        .format = render::PixelFormat::eRGBA8,
    };

    auto *pResource = ctx->createTextureRenderTarget(textureCreateInfo, kClearColour);
    rtvIndex = ctx->mapRenderTarget(pResource);

    pResource->setName("scene-target");

    setResource(pResource);
    setCurrentState(render::ResourceState::eShaderResource);
    setSrvIndex(ctx->mapTexture(pResource));
}

void SceneTargetHandle::destroy() {
    auto *pRtvHeap = ctx->getRtvHeap();
    auto *pSrvHeap = ctx->getSrvHeap();
    pRtvHeap->release(rtvIndex);
    pSrvHeap->release(getSrvIndex());
    delete getResource();
}

///
/// texture handle
///

TextureHandle::TextureHandle(RenderContext *ctx, std::string name)
    : ITextureHandle(ctx)
    , name(name)
{ }

void TextureHandle::create() {
    const auto& createInfo = ctx->getCreateInfo();
    assets::Image image = createInfo.depot.loadImage(name);

    const render::TextureInfo textureInfo = {
        .width = image.width,
        .height = image.height,

        .format = render::PixelFormat::eRGBA8
    };

    auto *pResource = ctx->createTexture(textureInfo);

    setResource(pResource);
    setSrvIndex(ctx->mapTexture(pResource));
    setCurrentState(render::ResourceState::eCopyDest);

    std::unique_ptr<render::UploadBuffer> pTextureStaging{ctx->createTextureUploadBuffer(textureInfo)};

    pResource->setName(name);
    pResource->setName("staging(" + name + ")");

    ctx->beginCopy();

    ctx->copyTexture(pResource, pTextureStaging.get(), textureInfo, image.data);

    ctx->endCopy();
}
