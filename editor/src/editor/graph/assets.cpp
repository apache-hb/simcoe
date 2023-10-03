#include "editor/graph/assets.h"

using namespace editor;
using namespace editor::graph;

void SwapChainHandle::create(RenderContext *ctx) {
    for (UINT i = 0; i < RenderContext::kBackBufferCount; ++i) {
        render::RenderTarget *pTarget = ctx->getRenderTarget(i);
        RenderTargetAlloc::Index rtvIndex = ctx->mapRenderTarget(pTarget);

        targets[i] = { pTarget, rtvIndex, render::ResourceState::ePresent };
    }
}

void SwapChainHandle::destroy(RenderContext *ctx) {
    for (UINT i = 0; i < RenderContext::kBackBufferCount; ++i) {
        delete targets[i].pRenderTarget;
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
    pResource->setName("scene target");

    currentState = render::ResourceState::eShaderResource;
    rtvIndex = ctx->mapRenderTarget(pResource);
    srvIndex = ctx->mapTexture(pResource);
}

void SceneTargetHandle::destroy(RenderContext *ctx) {
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
    pResource->setName(name);

    srvIndex = ctx->mapTexture(pResource);
    currentState = render::ResourceState::eCopyDest;

    std::unique_ptr<render::UploadBuffer> pTextureStaging{ctx->createTextureUploadBuffer(textureInfo)};
    pTextureStaging->setName(name + " staging");

    ctx->beginCopy();

    ctx->copyTexture(pResource, pTextureStaging.get(), textureInfo, image.data);

    ctx->endCopy();
}

void TextureHandle::destroy(RenderContext *ctx) {
    delete pResource;
}
