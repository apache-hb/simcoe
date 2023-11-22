#include "game/render/hud.h"

#include "engine/depot/service.h"

#include "stb/stb_rectpack.h"

using simcoe::DepotService;

using namespace game::render;

namespace game_ui = game::ui;

///
/// font handle
///

namespace {
    UINT getWindowDpi(const RenderCreateInfo& createInfo) {
        return GetDpiForWindow(createInfo.hWindow);
    }

    depot::Font loadFont(const RenderCreateInfo& createInfo, int pt, const fs::path& name) {
        depot::Font font = { DepotService::formatPath(name) };
        UINT dpi = getWindowDpi(createInfo);
        font.setFontSize(pt, dpi, dpi);
        return font;
    }
}

// font atlas handle

FontAtlasHandle::FontAtlasHandle(Graph *pGraph, std::span<FontAtlasInfo> fontInfos)
    : ISingleResourceHandle(pGraph, "font.atlas")
{ 
    constexpr auto kAtlasWidth = 512;
    constexpr auto kAtlasHeight = 512;

    size_t totalNodes = 0;
    for (auto& [path, pt, runes] : fontInfos) {
        totalNodes += runes.size();
    }

    stbrp_context stb;
    core::UniquePtr<stbrp_node[]> pNodes = totalNodes;
    core::UniquePtr<stbrp_rect[]> pRects = totalNodes;

    stbrp_init_target(&stb, kAtlasWidth, kAtlasHeight, pNodes, int(totalNodes));

    fonts.reserve(fontInfos.size());

    // first we get the size of each glyph
    // and pack the rects to the atlas
    size_t index = 0;
    for (auto& [path, pt, runes] : fontInfos) {
        depot::Font font = loadFont(ctx->getCreateInfo(), int(pt), path);
        for (char32_t rune : runes) {
            auto [w, h] = font.getGlyphSize(rune).as<int>();
            stbrp_rect rect = { 
                .id = int(rune),
                .w = w + 2,
                .h = h + 2
            };

            pRects[index++] = rect;
        }

        fonts.push_back(std::move(font));
    }

    int all_packed = stbrp_pack_rects(&stb, pRects, int(totalNodes));

    if (!all_packed) {
        LOG_ERROR("failed to pack all glyphs into atlas");
        return;
    }

    // then we render each glyph to the atlas
    // at the chosen rect position and save the uv coords

    bitmap = size2{ kAtlasWidth, kAtlasHeight };

    size_t runeIndex = 0;
    size_t fontIndex = 0;
    for (auto& [path, pt, runes] : fontInfos) {
        depot::Font& font = fonts[fontIndex++];
        for (char32_t rune : runes) {
            stbrp_rect rect = pRects[runeIndex++];

            SM_ASSERTF(rect.was_packed, "glyph `{}` was not packed", char(rune));
            SM_ASSERTF(rect.id == int(rune), "glyph `{}` was packed out of order", char(rune));

            // do this math to give everything a 1px border
            depot::CanvasPoint glyphPoint = { size_t(rect.x + 1), size_t(rect.y + 1) };
            depot::CanvasPoint glyphSize = { size_t(rect.w - 1), size_t(rect.h - 1) };

            font.drawGlyph(rune, glyphPoint, bitmap);

            // calculate uv coords 
            float u0 = float(rect.x) / float(kAtlasWidth);
            float v0 = float(rect.y) / float(kAtlasHeight);

            float u1 = float(rect.x + rect.w) / float(kAtlasWidth);
            float v1 = float(rect.y + rect.h) / float(kAtlasHeight);

            glyphs[rune] = { 
                .uvBounds = { { u0, v0 }, { u1, v1 } },
                .size = glyphSize.as<uint32_t>() - 1
            };

            LOG_DEBUG("glyph `{}`: rect=({},{},{},{}) bounds=({},{},{},{})", char(rune), rect.x, rect.y, rect.w, rect.h, u0, v0, u1, v1);
        }
    }

    // make top left pure white
    std::fill(bitmap.data.data(), bitmap.data.data() + 16, std::byte(0xFF));
}

void FontAtlasHandle::create() {
    const rhi::TextureInfo textureInfo = {
        .width = bitmap.size.width,
        .height = bitmap.size.height,

        .format = rhi::TypeFormat::eRGBA8
    };

    auto *pTexture = ctx->createTexture(textureInfo);

    setResource(pTexture);
    setSrvIndex(ctx->mapTexture(pTexture));
    setCurrentState(rhi::ResourceState::eCopyDest);

    std::unique_ptr<rhi::UploadBuffer> pTextureStaging{ctx->createTextureUploadBuffer(textureInfo)};

    pTexture->setName("atlas");
    pTextureStaging->setName("atlas.staging");

    ctx->beginCopy();

    ctx->copyTexture(pTexture, pTextureStaging.get(), textureInfo, bitmap.data);

    ctx->endCopy();
}

void FontAtlasHandle::destroy() {
    ISingleSRVHandle::destroy(ctx);
    ISingleResourceHandle::destroy();
}

depot::Text FontAtlasHandle::getTextShaper(size_t idx) {
    return { &fonts[idx] };
}

constexpr auto kTableFlags = ImGuiTableFlags_BordersV
                            | ImGuiTableFlags_BordersOuterH
                            | ImGuiTableFlags_Resizable
                            | ImGuiTableFlags_RowBg
                            | ImGuiTableFlags_NoBordersInBody;

void FontAtlasHandle::draw() {
    const auto *pSrvHeap = ctx->getSrvHeap();

    if (ImGui::BeginChild("Bounds", ImVec2(512.f, 512.f), ImGuiChildFlags_Border)) {
        if (ImGui::BeginTable("Glyphs", 2, kTableFlags)) {
            ImGui::TableSetupColumn("Glyph");
            ImGui::TableSetupColumn("Bounds");

            ImGui::TableHeadersRow();

            for (auto& [codepoint, glyph] : glyphs) {
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("%c", codepoint);

                ImGui::TableNextColumn();
                auto bounds = glyph.uvBounds;
                ImGui::Text("min: %f, %f\nmax: %f, %f", bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y);
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::Image(
        (ImTextureID)pSrvHeap->deviceOffset(getSrvIndex()), 
        { 512, 512 },
        { 0, 0 },
        { 1, 1 },
        { 1, 1, 1, 1 },
        { 1, 1, 1, 1 }
    );
}