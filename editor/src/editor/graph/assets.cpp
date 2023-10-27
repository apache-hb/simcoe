#include "editor/graph/assets.h"

#include "game/game.h"

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

    LOG_INFO("texture {} ({}x{})", name, image.width, image.height);
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
    UINT getWindowDpi(const RenderCreateInfo& createInfo) {
        return GetDpiForWindow(createInfo.hWindow);
    }

    assets::Font loadFont(const RenderCreateInfo& createInfo, std::string_view name) {
        assets::Font font = createInfo.depot.loadFont(name);
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
    font.setFontSize(pt, getWindowDpi(ctx->getCreateInfo()));
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

void TextHandle::debug() {
    static int size[2] = { int(bitmap.width), int(bitmap.height) };
    ImGui::InputInt2("size", size);

    static int startPos[2] = { int(start.x), int(start.y) };
    ImGui::InputInt2("start", startPos);

    static int pt = 32;
    ImGui::InputInt("pt", &pt);

    if (ImGui::Button("draw")) {
        game::Instance *pInstance = game::getInstance();

        pInstance->pRenderQueue->add("update-text", [this] {
            this->size = { size_t(size[0]), size_t(size[1]) };
            this->start = { size_t(startPos[0]), size_t(startPos[1]) };

            setFontSize(pt);
            draw();
            upload();
        });
    }

    auto offset = ctx->getSrvHeap()->deviceOffset(getSrvIndex());

    float aspect = float(bitmap.width) / float(bitmap.height);
    float availWidth = ImGui::GetWindowWidth() - 32;
    float availHeight = ImGui::GetWindowHeight() - 32;

    float totalWidth = availWidth;
    float totalHeight = availHeight;

    if (availWidth > availHeight * aspect) {
        totalWidth = availHeight * aspect;
    } else {
        totalHeight = availWidth / aspect;
    }

    ImGui::Image(
        /*user_texture_id=*/ (ImTextureID)offset,
        /*size=*/ { totalWidth, totalHeight },
        /*uv0=*/ ImVec2(0, 0),
        /*uv1=*/ ImVec2(1, 1),
        /*tint_col=*/ ImVec4(1.f, 1.f, 1.f, 1.f),
        /*border_col=*/ ImVec4(1.f, 1.f, 1.f, 1.f)
    );
}
