#include "game/render/hud/layout.h"

#include "engine/depot/service.h"

using simcoe::DepotService;

using namespace game::ui;

namespace game_ui = game::ui;

using Layout = game_ui::Context;

Layout::Context(BoxBounds screen, size_t dpi)
    : screen(screen)
    , user(screen)
    , dpi(dpi)
{ }


void Layout::begin() {
    vertices.clear();
    indices.clear();
}

void Layout::box(const BoxBounds& bounds, uint8x4 colour) {
    UiVertex newVertices[4] = {
        { bounds.min, 0.f, colour },
        { { bounds.max.x, bounds.min.y }, 0.f, colour },
        { { bounds.min.x, bounds.max.y }, 0.f, colour },
        { bounds.max, 0.f, colour },
    };

    UiIndex newIndices[6] = {
        0, 1, 2,
        2, 1, 3,
    };

    vertices.insert(vertices.end(), std::begin(newVertices), std::end(newVertices));
    indices.insert(indices.end(), std::begin(newIndices), std::end(newIndices));
}

void Layout::text(const BoxBounds& bounds, uint8x4 colour, const TextDrawInfo& info) {
    auto *pFont = getFont(info.size);

    auto image = pFont->drawText(utf8::StaticText(info.text), { 0, 0 }, { 0, 0 });

    float2 size = image.size.as<float>();

    float2 offset = { 0, 0 };
    switch (info.align.h) {
    case AlignH::eLeft: offset.x = 0; break;
    case AlignH::eCenter: offset.x = (bounds.max.x - bounds.min.x - size.x) / 2.f; break;
    case AlignH::eRight: offset.x = bounds.max.x - bounds.min.x - size.x; break;
    default: SM_NEVER("invalid align"); break;
    }

    switch (info.align.v) {
    case AlignV::eTop: offset.y = 0; break;
    case AlignV::eMiddle: offset.y = (bounds.max.y - bounds.min.y - size.y) / 2.f; break;
    case AlignV::eBottom: offset.y = bounds.max.y - bounds.min.y - size.y; break;
    default: SM_NEVER("invalid align"); break;
    }

    UiVertex newVertices[4] = {
        { bounds.min + offset, 0.f, colour },
        { { bounds.max.x + offset.x, bounds.min.y + offset.y }, 0.f, colour },
        { { bounds.min.x + offset.x, bounds.max.y + offset.y }, 0.f, colour },
        { bounds.max + offset, 0.f, colour },
    };

    UiIndex newIndices[6] = {
        0, 1, 2,
        2, 1, 3,
    };

    vertices.insert(vertices.end(), std::begin(newVertices), std::end(newVertices));
    indices.insert(indices.end(), std::begin(newIndices), std::end(newIndices));
}

depot::Font *Layout::getFont(size_t size) {
    if (auto it = fonts.find(size); it != fonts.end()) 
        return &it->second;

    auto [it, ok] = fonts.emplace(size, DepotService::getAssetPath("SwarmFace-Regular.ttf"));
    SM_ASSERTF(ok, "failed to create font");

    depot::Font *pFont = &it->second;
    pFont->setFontSize(int(size), int(dpi));
    return &it->second;
}
