#include "engine/assets/font.h"

#include "engine/math/math.h"

#include "engine/engine.h"

using namespace simcoe;
using namespace simcoe::assets;

namespace {
    constexpr float kAngle = (0.f / 360.f) * 2.f * math::kPi<float>;
    constexpr math::Resolution<int> kScreenSize = { 1280, 720 };

    void bltGlyph(Image& image, FT_Bitmap *pBitmap, FT_Int x, FT_Int y) {
        ASSERTF(pBitmap->pixel_mode == FT_PIXEL_MODE_GRAY, "unsupported pixel mode (mode={})", pBitmap->pixel_mode);

        for (FT_Int row = 0; row < pBitmap->rows; row++) {
            for (FT_Int col = 0; col < pBitmap->width; col++) {
                FT_Int index = row * pBitmap->pitch + col;
                FT_Byte alpha = pBitmap->buffer[index];

                FT_Int imageIndex = (y + row) * image.width + (x + col);
                image.data[imageIndex * 4 + 0] = std::byte(255);
                image.data[imageIndex * 4 + 1] = std::byte(255);
                image.data[imageIndex * 4 + 2] = std::byte(255);
                image.data[imageIndex * 4 + 3] = std::byte(alpha);
            }
        }
    }
}

Font::Font(const char *path) {
    FT_Error error = FT_Init_FreeType(&library);
    if (error) {
        logAssert("failed to initialize FreeType library (fterr={})", FT_Error_String(error));
    }

    error = FT_New_Face(library, path, 0, &face);
    if (error) {
        logAssert("failed to load font face from `{}` (fterr={})", path, FT_Error_String(error));
    }

    error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    if (error) {
        logAssert("failed to select unicode charmap (fterr={})", FT_Error_String(error));
    }
}

Font::~Font() {
    FT_Done_Face(face);
    FT_Done_FreeType(library);
}

void Font::setFontSize(int newPt, int newDpi) {
    if (pt == newPt && dpi == newDpi)
        return;

    pt = newPt;
    dpi = newDpi;

    simcoe::logInfo("setting font size to {}pt (dpi={})", pt, dpi);
    FT_Error error = FT_Set_Char_Size(face, pt * 64, 0, dpi, 0);
    if (error) {
        logAssert("failed to set font size (fterr={})", FT_Error_String(error));
    }
}

Image Font::drawText(utf8::StaticText text) {
    FT_GlyphSlot slot = face->glyph;
    FT_Matrix matrix = {
        .xx = FT_Fixed(cos(kAngle) * 0x10000),
        .xy = FT_Fixed(-sin(kAngle) * 0x10000),
        .yx = FT_Fixed(sin(kAngle) * 0x10000),
        .yy = FT_Fixed(cos(kAngle) * 0x10000)
    };

    FT_Vector pen = {
        .x = 8 * 64,
        .y = (kScreenSize.height - int(pt)) * 64
    };

    Image result = {
        .format = ImageFormat::eRGBA8,
        .width = kScreenSize.width,
        .height = kScreenSize.height,
        .data = std::vector<std::byte>(kScreenSize.width * kScreenSize.height * 4)
    };

    for (char32_t codepoint : text) {
        FT_Set_Transform(face, &matrix, &pen);

        if (FT_Error error = FT_Load_Char(face, codepoint, FT_LOAD_RENDER)) {
            logAssert("failed to load glyph (codepoint={}, fterr={})", uint32_t(codepoint), FT_Error_String(error));
        }

        FT_Bitmap *bitmap = &slot->bitmap;
        bltGlyph(result, bitmap, slot->bitmap_left, kScreenSize.height - slot->bitmap_top);

        pen.x += slot->advance.x;
        pen.y += slot->advance.y;
    }

    return result;
}
