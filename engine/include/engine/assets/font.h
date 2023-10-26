#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "engine/assets/image.h"

#include "engine/core/utf8.h"

namespace simcoe::assets {
    struct Font {
        Font(const char *path);
        ~Font();

        void setFontSize(int pt, int dpi);

        Image drawText(utf8::StaticText text);

    private:
        FT_Face face;

        int pt;
        int dpi;
    };
}
