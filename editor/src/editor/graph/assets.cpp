#include "editor/graph/assets.h"

using namespace editor;
using namespace editor::graph;

static constexpr math::float4 kRenderClearColour = { 0.0f, 0.0f, 0.0f, 1.0f };
static constexpr math::float4 kSceneClearColour = { 0.0f, 0.0f, 0.0f, 1.0f };

SwapChainHandle::SwapChainHandle(Graph *ctx)
    : IResourceHandle(ctx, "swapchain.rtv", StateDep(eDepDisplaySize | eDepBackBufferCount))
{
    setClearColour(kRenderClearColour);
}

void SwapChainHandle::create() {
    const auto &createInfo = ctx->getCreateInfo();
    targets.resize(createInfo.backBufferCount);

    for (UINT i = 0; i < createInfo.backBufferCount; i++) {
        rhi::RenderTarget *pTarget = ctx->getRenderTarget(i);
        RenderTargetAlloc::Index rtvIndex = ctx->mapRenderTarget(pTarget);

        setResourceState(pTarget, rhi::ResourceState::ePresent);

        pTarget->setName("swapchain-target-" + std::to_string(i));

        targets[i] = { pTarget, rtvIndex };
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

rhi::DeviceResource *SwapChainHandle::getResource() const {
    return targets[ctx->getFrameIndex()].pRenderTarget;
}

RenderTargetAlloc::Index SwapChainHandle::getRtvIndex() const {
    return targets[ctx->getFrameIndex()].rtvIndex;
}

///
/// scene target handle
///

SceneTargetHandle::SceneTargetHandle(Graph *ctx)
    : ISingleResourceHandle(ctx, "scene.srv.rtv", eDepRenderSize)
{
    setClearColour(kSceneClearColour);
}

void SceneTargetHandle::create() {
    const auto& createInfo = ctx->getCreateInfo();

    const rhi::TextureInfo textureCreateInfo = {
        .width = createInfo.renderWidth,
        .height = createInfo.renderHeight,
        .format = rhi::TypeFormat::eRGBA8,
    };

    auto *pResource = ctx->createTextureRenderTarget(textureCreateInfo, getClearColour());
    pResource->setName("scene-target");

    setResource(pResource);
    setCurrentState(rhi::ResourceState::eShaderResource);
    setSrvIndex(ctx->mapTexture(pResource));
    setRtvIndex(ctx->mapRenderTarget(pResource));
}

void SceneTargetHandle::destroy() {
    ISingleSRVHandle::destroy(ctx);
    ISingleRTVHandle::destroy(ctx);
    ISingleResourceHandle::destroy();
}

///
/// depth handle
///

void DepthTargetHandle::create() {
    const auto& createInfo = ctx->getCreateInfo();

    const rhi::TextureInfo textureCreateInfo = {
        .width = createInfo.renderWidth,
        .height = createInfo.renderHeight,
        .format = rhi::TypeFormat::eDepth32,
    };

    auto *pResource = ctx->createDepthStencil(textureCreateInfo);
    setResource(pResource);
    setCurrentState(rhi::ResourceState::eDepthWrite);
    setDsvIndex(ctx->mapDepth(pResource));
}

void DepthTargetHandle::destroy() {
    ISingleDSVHandle::destroy(ctx);
    ISingleResourceHandle::destroy();
}

///
/// texture handle
///

TextureHandle::TextureHandle(Graph *ctx, std::string name)
    : ISingleResourceHandle(ctx, name)
    , name(name)
{ }

void TextureHandle::create() {
    const auto& createInfo = ctx->getCreateInfo();
    assets::Image image = createInfo.depot.loadImage(name);

    const rhi::TextureInfo textureInfo = {
        .width = image.width,
        .height = image.height,

        .format = rhi::TypeFormat::eRGBA8
    };

    simcoe::logInfo("texture {} ({}x{})", name, image.width, image.height);

    auto *pResource = ctx->createTexture(textureInfo);

    setResource(pResource);
    setSrvIndex(ctx->mapTexture(pResource));
    setCurrentState(rhi::ResourceState::eCopyDest);

    std::unique_ptr<rhi::UploadBuffer> pTextureStaging{ctx->createTextureUploadBuffer(textureInfo)};

    pResource->setName(name);
    pTextureStaging->setName("staging(" + name + ")");

    ctx->beginCopy();

    ctx->copyTexture(pResource, pTextureStaging.get(), textureInfo, image.data);

    ctx->endCopy();
}

void TextureHandle::destroy() {
    ISingleSRVHandle::destroy(ctx);
    ISingleResourceHandle::destroy();
}
