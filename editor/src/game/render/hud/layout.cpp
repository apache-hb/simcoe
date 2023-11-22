#include "game/render/hud/layout.h"

#include "engine/depot/service.h"
#include "engine/log/service.h"

using namespace game::ui;

namespace math = simcoe::math;
namespace game_ui = game::ui;

using Layout = game_ui::Context;

void TextWidget::draw(Context *pContext, const DrawInfo& info) const {
    TextDrawInfo drawInfo = { text, align, scale };
    BoxBounds box = pContext->text(info.bounds, info.colour, drawInfo);
    if (bDrawBox) {
        float border = 2.f;
        float2 min = box.min - border;
        float2 max = box.max + border;

        // draw a border around the text
        BoxBounds top = { min, { max.x, min.y + border} };
        BoxBounds bottom = { { min.x, max.y - border }, max };
        BoxBounds left = { min, { min.x + border, max.y } };
        BoxBounds right = { { max.x - border, min.y }, max };

        pContext->box(top, { 255, 0, 0, 255 });
        pContext->box(bottom, { 255, 0, 0, 255 });
        pContext->box(left, { 255, 0, 0, 255 });
        pContext->box(right, { 255, 0, 0, 255 });
    }
}

Layout::Context(BoxBounds screenBounds)
    : screen(screenBounds)
    , user(screenBounds)
{ }

void Layout::begin(IWidget *pWidget) {
    vertices.clear();
    indices.clear();

    DrawInfo info = {
        .bounds = user,
        .colour = { 255, 255, 255, 255 },
    };

    pWidget->draw(this, info);
}

void Layout::box(const BoxBounds& bounds, uint8x4 colour) {
    UiVertex newVertices[4] = {
        { bounds.min, 0.f, colour },
        { { bounds.max.x, bounds.min.y }, 0.f, colour },
        { { bounds.min.x, bounds.max.y }, 0.f, colour },
        { bounds.max, 0.f, colour },
    };

    size_t indexOffset = vertices.size();

    UiIndex newIndices[6] = {
        UiIndex(0u + indexOffset), UiIndex(1u + indexOffset), UiIndex(2u + indexOffset),
        UiIndex(2u + indexOffset), UiIndex(1u + indexOffset), UiIndex(3u + indexOffset),
    };

    vertices.insert(vertices.end(), std::begin(newVertices), std::end(newVertices));
    indices.insert(indices.end(), std::begin(newIndices), std::end(newIndices));
}

BoxBounds Layout::text(const BoxBounds& inBounds, uint8x4 colour, const TextDrawInfo& info) {
    // if the max bounds are 0,0 we use the available screen bounds
    BoxBounds bounds = inBounds;
    if (bounds.max == 0.f) {
        bounds.max = screen.max;
    }

    auto iter = shaper.shape(info.text);

    // the bounds of the text relative to @bounds
    float2 textMin = 0.f;
    float2 textMax = 0.f;

    // keep the first index of the text in case we need to 
    // go back and move it
    size_t firstVertexIndex = vertices.size();

    // the current position of the cursor relative to the top left of the text
    float2 cursor = 0.f;

    size_t index = firstVertexIndex;

    auto tbegin = info.text.begin();
    auto tend = info.text.end();

    auto gbegin = iter.begin();
    auto gend = iter.end();

    for (; gbegin != gend && tbegin != tend; ++gbegin, ++tbegin) {
        const auto glyph = *gbegin;
        const auto codepoint = *tbegin;

        float fxo = float(glyph.xOffset) * info.scale;
        float fyo = float(glyph.yOffset) * info.scale;
        float fxa = float(glyph.xAdvance) * info.scale;
        float fya = float(glyph.yAdvance) * info.scale;

        // cant render this glyph
        if (!atlas.contains(codepoint)) {
            cursor.x += fxa;
            cursor.y += fya;
            continue;
        }

        auto [texBounds, size] = atlas.at(codepoint);

        float2 glyphSize = size.as<float>() * info.scale;
        float2 glyphStart = cursor + float2(fxo, fyo);

        // harfbuzz gives us the glyph position relative to the baseline
        // but our uv coords are relative to the top left of the glyph

        float2 glyphTopLeft = glyphStart - float2(0.f, glyphSize.y);
        float2 glyphBottomRight = glyphStart + float2(glyphSize.x, 0.f);

        // create the quad for this glyph
        UiVertex newVertices[4] = {
            // bottom left
            { glyphStart, { texBounds.min.x, texBounds.max.y }, colour },
            // bottom right
            { { glyphStart.x + glyphSize.x, glyphStart.y }, { texBounds.max.x, texBounds.max.y }, colour },
            // top left
            { { glyphStart.x, glyphStart.y + glyphSize.y }, { texBounds.min.x, texBounds.min.y }, colour },
            // top right
            { glyphStart + glyphSize, { texBounds.max.x, texBounds.min.y }, colour },
        };

        UiIndex indexOffset = UiIndex(index);

        UiIndex newIndices[6] = {
            UiIndex(0u + indexOffset), UiIndex(1u + indexOffset), UiIndex(2u + indexOffset),
            UiIndex(2u + indexOffset), UiIndex(1u + indexOffset), UiIndex(3u + indexOffset),
        };

        index += 4;

        // track the bounds of the text
        textMin = math::min(textMin, glyphTopLeft);
        textMax = math::max(textMax, glyphBottomRight);

        // add the quad and indices to the draw list
        vertices.insert(vertices.end(), std::begin(newVertices), std::end(newVertices));
        indices.insert(indices.end(), std::begin(newIndices), std::end(newIndices));

        // move the cursor to the next glyph
        cursor.x += fxa;
        cursor.y += fya;
    }

    float2 offset = bounds.min + textMin;

    switch (info.align.v) {
    case AlignV::eTop:
        break;
    case AlignV::eMiddle:
        offset.y -= (bounds.max.y - bounds.min.y) / 2.f - (textMax.y - textMin.y) / 2.f;
        break;
    case AlignV::eBottom:
        offset.y -= bounds.max.y - textMax.y;
        break;
    default: SM_NEVER("invalid align");
    }

    switch (info.align.h) {
    case AlignH::eLeft:
        break;
    case AlignH::eCenter:
        offset.x += (bounds.max.x - bounds.min.x) / 2.f - (textMax.x - textMin.x) / 2.f;
        break;
    case AlignH::eRight:
        offset.x += bounds.max.x - textMax.x;
        break;
    default: SM_NEVER("invalid align");
    }

    // move the text to the correct position
    for (size_t i = firstVertexIndex; i < vertices.size(); i++) {
        vertices[i].position += offset;
    }

    return { offset - textMin, textMax + offset };
}
