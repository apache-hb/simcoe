#include "editor/graph/assets.h"

#include "engine/depot/service.h"

using namespace editor;
using namespace editor::graph;

using namespace simcoe;

static constexpr math::float4 kRenderClearColour = { 0.0f, 0.0f, 0.0f, 1.0f };
static constexpr math::float4 kSceneClearColour = { 0.0f, 0.0f, 0.0f, 1.0f };

SwapChainHandle::SwapChainHandle(Graph *pGraph)
    : IResourceHandle(pGraph, "swapchain.rtv", StateDep(eDepDisplaySize | eDepBackBufferCount))
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

    auto *pRenderTexture = ctx->createTextureRenderTarget(textureCreateInfo, getClearColour());
    pRenderTexture->setName("scene-target");

    setResource(pRenderTexture);
    setCurrentState(rhi::ResourceState::eTextureRead);
    setSrvIndex(ctx->mapTexture(pRenderTexture));
    setRtvIndex(ctx->mapRenderTarget(pRenderTexture));
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

    auto *pDepthStencil = ctx->createDepthStencil(textureCreateInfo);
    setResource(pDepthStencil);
    setCurrentState(rhi::ResourceState::eDepthWrite);
    setDsvIndex(ctx->mapDepth(pDepthStencil));
}

void DepthTargetHandle::destroy() {
    ISingleDSVHandle::destroy(ctx);
    ISingleResourceHandle::destroy();
}

///
/// texture handle
///

TextureHandle::TextureHandle(Graph *pGraph, std::string name)
    : ISingleResourceHandle(pGraph, name)
    , name(name)
{
    image = DepotService::loadImage(name);

    LOG_INFO("texture {} ({}x{})", name, image.width, image.height);
}

void TextureHandle::create() {
    const rhi::TextureInfo textureInfo = {
        .width = image.width,
        .height = image.height,

        .format = rhi::TypeFormat::eRGBA8
    };

    auto *pTexture = ctx->createTexture(textureInfo);

    setResource(pTexture);
    setSrvIndex(ctx->mapTexture(pTexture));
    setCurrentState(rhi::ResourceState::eCopyDest);

    std::unique_ptr<rhi::UploadBuffer> pTextureStaging{ctx->createTextureUploadBuffer(textureInfo)};

    pTexture->setName(name);
    pTextureStaging->setName("staging(" + name + ")");

    ctx->beginCopy();

    ctx->copyTexture(pTexture, pTextureStaging.get(), textureInfo, image.data);

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
    UINT getWindowDpi(const RenderCreateInfo& createInfo) {
        return GetDpiForWindow(createInfo.hWindow);
    }

    depot::Font loadFont(const RenderCreateInfo& createInfo, std::string_view name) {
        depot::Font font = DepotService::loadFont(name);
        UINT dpi = getWindowDpi(createInfo);
        font.setFontSize(32, dpi);
        return font;
    }
}

TextHandle::TextHandle(Graph *ctx, std::string_view ttf)
    : ISingleResourceHandle(ctx, std::string(ttf))
    , ttf(ttf)
    , font(loadFont(ctx->getCreateInfo(), ttf))
{
    draw();
}

void TextHandle::create() {
    upload();
}

void TextHandle::destroy() {
    ISingleSRVHandle::destroy(ctx);
    ISingleResourceHandle::destroy();
}

void TextHandle::setFontSize(size_t pt) {
    font.setFontSize(core::intCast<int>(pt), getWindowDpi(ctx->getCreateInfo()));
}

void TextHandle::draw() {
    bitmap = font.drawText(segments, start, size);
    LOG_INFO("font (ttf={}, bitmap={}x{})", ttf, bitmap.width, bitmap.height);
}

void TextHandle::upload() {
    const rhi::TextureInfo textureInfo = {
        .width = bitmap.width,
        .height = bitmap.height,

        .format = rhi::TypeFormat::eRGBA8
    };

    auto *pTexture = ctx->createTexture(textureInfo);

    setResource(pTexture);
    setSrvIndex(ctx->mapTexture(pTexture));
    setCurrentState(rhi::ResourceState::eCopyDest);

    std::unique_ptr<rhi::UploadBuffer> pTextureStaging{ctx->createTextureUploadBuffer(textureInfo)};

    pTexture->setName("text");
    pTextureStaging->setName("staging(text)");

    ctx->beginCopy();

    ctx->copyTexture(pTexture, pTextureStaging.get(), textureInfo, bitmap.data);

    ctx->endCopy();
}

