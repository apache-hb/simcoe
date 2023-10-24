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
    setCurrentState(rhi::ResourceState::eTextureRead);
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

TextureHandle::TextureHandle(Graph *graph, std::string name)
    : ISingleResourceHandle(graph, name)
    , name(name)
{
    const auto& createInfo = ctx->getCreateInfo();
    image = createInfo.depot.loadImage(name);

    simcoe::logInfo("texture {} ({}x{})", name, image.width, image.height);
}

void TextureHandle::create() {
    const rhi::TextureInfo textureInfo = {
        .width = image.width,
        .height = image.height,

        .format = rhi::TypeFormat::eRGBA8
    };

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


///
/// font handle
///

namespace {
    assets::Font loadFont(const RenderCreateInfo& createInfo, std::string_view name) {
        assets::Font font = createInfo.depot.loadSystemFont(name);
        UINT dpi = GetDpiForWindow(createInfo.hWindow);
        font.setFontSize(32, dpi);
        return font;
    }
}

TextHandle::TextHandle(Graph *ctx, std::string_view name, std::u32string text)
    : ISingleResourceHandle(ctx, std::string(name))
    , font(loadFont(ctx->getCreateInfo(), name))
    , text(text)
{
    bitmap = font.drawText(text);
    simcoe::logInfo("font (ttf={}, bitmap={}x{})", name, bitmap.width, bitmap.height);
}

void TextHandle::create() {
    const rhi::TextureInfo textureInfo = {
        .width = bitmap.width,
        .height = bitmap.height,

        .format = rhi::TypeFormat::eRGBA8
    };

    auto *pResource = ctx->createTexture(textureInfo);

    setResource(pResource);
    setSrvIndex(ctx->mapTexture(pResource));
    setCurrentState(rhi::ResourceState::eCopyDest);

    std::unique_ptr<rhi::UploadBuffer> pTextureStaging{ctx->createTextureUploadBuffer(textureInfo)};

    pResource->setName("text");
    pTextureStaging->setName("staging(text)");

    ctx->beginCopy();

    ctx->copyTexture(pResource, pTextureStaging.get(), textureInfo, bitmap.data);

    ctx->endCopy();
}

void TextHandle::destroy() {
    ISingleSRVHandle::destroy(ctx);
    ISingleResourceHandle::destroy();
}
