#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "engine/assets/image.h"

#include <string_view>

namespace simcoe::assets {
    struct Font {
        Font(const char *path);
        ~Font();

        void setFontSize(int pt, int dpi);

        Image drawText(std::u32string_view text);

    private:
        FT_Library library;
        FT_Face face;

        int pt;
        int dpi;
    };
}
