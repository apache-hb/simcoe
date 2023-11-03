#include "engine/depot/font.h"

#include "engine/core/units.h"
#include "engine/service/freetype.h"
#include "engine/log/service.h"

#include "engine/math/math.h"

using namespace simcoe;
using namespace simcoe::depot;
using namespace simcoe::math;

namespace {
    constexpr float getAngle(float deg) {
        return (deg / 360.f) * 2.f * math::kPi<float>;
    }

    constexpr math::float4 kBlack = math::float4(1.f, 1.f, 1.f, 1.f);

    void bltGlyph(Image& image, FT_Bitmap *pBitmap, FT_UInt x, FT_UInt y, math::float4 colour) {
        ASSERTF(pBitmap->pixel_mode == FT_PIXEL_MODE_GRAY, "unsupported pixel mode (mode={})", pBitmap->pixel_mode);

        auto writePixel = [&](size_t x, size_t y, std::byte value) {
            size_t index = (y * image.width + x) * 4;
            if (index + 3 > image.data.size()) return;

            image.data[index + 0] = std::byte(255 * colour.x);
            image.data[index + 1] = std::byte(255 * colour.y);
            image.data[index + 2] = std::byte(255 * colour.z);
            image.data[index + 3] = value;
        };

        for (FT_UInt row = 0; row < pBitmap->rows; row++) {
            for (FT_UInt col = 0; col < pBitmap->width; col++) {
                FT_UInt index = row * pBitmap->pitch + col;
                FT_Byte alpha = pBitmap->buffer[index];

                writePixel(x + col, y + row, std::byte(alpha));
            }
        }
    }

    struct FontRender {
        FontRender(FT_Face face, depot::CanvasPoint origin, depot::CanvasSize size, float deg, int pt)
            : face(face)
            , slot(face->glyph)
            , origin(origin)
            , size(size)
        {
            setMatrixAngle(deg);
            setPen(pt);

            height = face->size->metrics.height;

            image = {
                .format = ImageFormat::eRGBA8,
                .width = size.width,
                .height = size.height,
                .data = std::vector<std::byte>(size.width * size.height * 4)
            };
        }

        void setMatrixAngle(float deg) {
            float angle = getAngle(deg);
            matrix = {
                .xx = FT_Fixed(cos(angle) * 0x10000),
                .xy = FT_Fixed(-sin(angle) * 0x10000),
                .yx = FT_Fixed(sin(angle) * 0x10000),
                .yy = FT_Fixed(cos(angle) * 0x10000)
            };
        }

        void setPen(int pt) {
            pen = {
                .x = int(origin.x) * 64,
                .y = (int(origin.y) + int(size.height) - int(pt)) * 64
            };
        }

        void setTransform() {
            FT_Set_Transform(face, &matrix, &pen);
        }

        void draw(char32_t codepoint, math::float4 colour) {
            if (FT_Error error = FT_Load_Char(face, codepoint, FT_LOAD_RENDER)) {
                LOG_ASSERT("failed to load glyph (codepoint={}, fterr={})", uint32_t(codepoint), FT_Error_String(error));
            }

            FT_Bitmap *bitmap = &slot->bitmap;
            bltGlyph(image, bitmap, slot->bitmap_left, core::intCast<FT_UInt>(size.height) - slot->bitmap_top, colour);
        }

        void advance() {
            pen.x += slot->advance.x;
            pen.y += slot->advance.y;
        }

        void newline() {
            pen.x = int(origin.x) * 64;
            pen.y -= height;
        }

        FT_Face face;
        FT_GlyphSlot slot;

        CanvasPoint origin;
        CanvasSize size;
        FT_Matrix matrix;
        FT_Vector pen;

        FT_Pos height;

        Image image;
    };
}

Font::Font(const char *path) {
    FT_Library library = FreeTypeService::getLibrary();

    if (FT_Error error = FT_New_Face(library, path, 0, &face)) {
        LOG_ASSERT("failed to load font face from `{}` (fterr={})", path, FT_Error_String(error));
    }

    if (FT_Error error = FT_Select_Charmap(face, FT_ENCODING_UNICODE)) {
        LOG_ASSERT("failed to select unicode charmap (fterr={})", FT_Error_String(error));
    }
}

Font::~Font() {
    FT_Done_Face(face);
}

void Font::setFontSize(int newPt, int newDpi) {
    if (pt == newPt && dpi == newDpi)
        return;

    pt = newPt;
    dpi = newDpi;

    LOG_INFO("setting font size to {}pt (dpi={})", pt, dpi);
    if (FT_Error error = FT_Set_Char_Size(face, pt * 64, 0, dpi, 0)) {
        LOG_ASSERT("failed to set font size (fterr={})", FT_Error_String(error));
    }
}

Image Font::drawText(utf8::StaticText text, CanvasPoint origin, CanvasSize size, float deg) {
    FontRender render(face, origin, size, deg, pt);

    for (char32_t codepoint : text) {
        if (codepoint == '\n') {
            render.newline();
            continue;
        }

        render.setTransform();

        render.draw(codepoint, kBlack);

        render.advance();
    }

    return render.image;
}

Image Font::drawText(std::span<const TextSegment> segments, CanvasPoint origin, CanvasSize size, float deg) {
    FontRender render(face, origin, size, deg, pt);

    for (const TextSegment& segment : segments) {
        for (char32_t codepoint : segment.text) {
            if (codepoint == '\n') {
                render.newline();
                continue;
            }

            render.setTransform();

            render.draw(codepoint, segment.colour);

            render.advance();
        }
    }

    return render.image;
}
