#pragma once

#include "engine/depot/image.h"

#include "engine/service/freetype.h"

#include "engine/core/utf8.h"

#include "engine/math/math.h"

#include <span>

namespace simcoe::depot {
    using CanvasPoint = math::size2;
    using CanvasSize = math::size2;

    struct TextSegment {
        utf8::StaticText text;
        math::float4 colour = math::float4(1.f);
    };

    struct Font {
        SM_DEPRECATED("this is broken for some reason, use the filepath constructor for now")
        Font(std::shared_ptr<IFile> pFile);

        Font(const fs::path& path);

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
