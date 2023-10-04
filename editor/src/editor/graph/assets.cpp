#include "editor/graph/assets.h"

using namespace editor;
using namespace editor::graph;

void SwapChainHandle::create(RenderContext *ctx) {
    for (UINT i = 0; i < RenderContext::kBackBufferCount; i++) {
        render::RenderTarget *pTarget = ctx->getRenderTarget(i);
        RenderTargetAlloc::Index rtvIndex = ctx->mapRenderTarget(pTarget);

        pTarget->setName("swapchain-target-" + std::to_string(i));

        targets[i] = { pTarget, rtvIndex, render::ResourceState::ePresent };
    }
}

void SwapChainHandle::destroy(RenderContext *ctx) {
    auto *pRtvHeap = ctx->getRtvHeap();
    for (UINT i = 0; i < RenderContext::kBackBufferCount; ++i) {
        const auto& frame = targets[i];
        delete frame.pRenderTarget;
        pRtvHeap->release(frame.rtvIndex);
    }
}

render::ResourceState SwapChainHandle::getCurrentState(RenderContext *ctx) const {
    return targets[ctx->getFrameIndex()].state;
}

void SwapChainHandle::setCurrentState(RenderContext *ctx, render::ResourceState state) {
    targets[ctx->getFrameIndex()].state = state;
}

render::DeviceResource *SwapChainHandle::getResource(RenderContext *ctx) const {
    return targets[ctx->getFrameIndex()].pRenderTarget;
}

RenderTargetAlloc::Index SwapChainHandle::getRtvIndex(RenderContext *ctx) const {
    return targets[ctx->getFrameIndex()].rtvIndex;
}


///
/// scene target handle
///

void SceneTargetHandle::create(RenderContext *ctx) {
    const auto& createInfo = ctx->getCreateInfo();

    const render::TextureInfo textureCreateInfo = {
        .width = createInfo.renderWidth,
        .height = createInfo.renderHeight,
        .format = render::PixelFormat::eRGBA8,
    };

    pResource = ctx->createTextureRenderTarget(textureCreateInfo, kClearColour);
    currentState = render::ResourceState::eShaderResource;
    rtvIndex = ctx->mapRenderTarget(pResource);
    srvIndex = ctx->mapTexture(pResource);

    pResource->setName("scene-target");
}

void SceneTargetHandle::destroy(RenderContext *ctx) {
    auto *pRtvHeap = ctx->getRtvHeap();
    auto *pSrvHeap = ctx->getSrvHeap();
    pRtvHeap->release(rtvIndex);
    pSrvHeap->release(srvIndex);
    delete pResource;
}

///
/// texture handle
///

TextureHandle::TextureHandle(std::string name)
    : name(name)
{ }

void TextureHandle::create(RenderContext *ctx) {
    const auto& createInfo = ctx->getCreateInfo();
    assets::Image image = createInfo.depot.loadImage(name);

    const render::TextureInfo textureInfo = {
        .width = image.width,
        .height = image.height,

        .format = render::PixelFormat::eRGBA8
    };

    pResource = ctx->createTexture(textureInfo);
    srvIndex = ctx->mapTexture(pResource);
    currentState = render::ResourceState::eCopyDest;

    std::unique_ptr<render::UploadBuffer> pTextureStaging{ctx->createTextureUploadBuffer(textureInfo)};

    pResource->setName(name);
    pResource->setName("staging(" + name + ")");

    ctx->beginCopy();

    ctx->copyTexture(pResource, pTextureStaging.get(), textureInfo, image.data);

    ctx->endCopy();
}

void TextureHandle::destroy(RenderContext *ctx) {
    auto *pSrvHeap = ctx->getSrvHeap();

    pSrvHeap->release(srvIndex);
    delete pResource;
}
