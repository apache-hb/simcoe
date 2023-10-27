#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "engine/assets/image.h"

#include "engine/math/math.h"

#include "engine/core/utf8.h"

#include <span>

namespace simcoe::assets {
    using CanvasPoint = math::size2;
    using CanvasSize = math::Resolution<size_t>;

    struct TextSegment {
        utf8::StaticText text;
        math::float4 colour = math::float4(1.f);
    };

    struct Font {
        Font(const char *path);
        ~Font();

        void setFontSize(int pt, int dpi);

        Image drawText(utf8::StaticText text, CanvasPoint start, CanvasSize size, float angle = 0.f);
        Image drawText(std::span<const TextSegment> segments, CanvasPoint start, CanvasSize size, float angle = 0.f);

    private:
        FT_Face face;

        int pt;
        int dpi;
    };
}
