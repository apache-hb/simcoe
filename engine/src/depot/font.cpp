#include "engine/depot/font.h"

#include "engine/core/units.h"
#include "engine/service/freetype.h"
#include "engine/log/service.h"

#include "engine/math/math.h"

#include <hb.h>
#include <hb-ft.h>

#include "stb/stb_rectpack.h"

using namespace simcoe;
using namespace simcoe::depot;
using namespace simcoe::math;

namespace {
    constexpr float getAngle(float deg) {
        return (deg / 360.f) * 2.f * math::kPi<float>;
    }

    constexpr math::float4 kBlack = math::float4(1.f, 1.f, 1.f, 1.f);

    void bltGlyph(Image& image, FT_String *pName, FT_Bitmap *pBitmap, FT_UInt x, FT_UInt y, math::float4 colour) {
        auto writePixel = [&](size_t x, size_t y, std::byte value) {
            size_t index = (y * image.size.width + x) * 4;
            if (index + 3 > image.data.size()) return;

            image.data[index + 0] = std::byte(255 * colour.x);
            image.data[index + 1] = std::byte(255 * colour.y);
            image.data[index + 2] = std::byte(255 * colour.z);
            image.data[index + 3] = value;
        };

        if (pBitmap->pixel_mode == FT_PIXEL_MODE_GRAY) {
            for (FT_UInt row = 0; row < pBitmap->rows; row++) {
                for (FT_UInt col = 0; col < pBitmap->width; col++) {
                    FT_UInt index = row * pBitmap->pitch + col;
                    FT_Byte alpha = pBitmap->buffer[index];

                    writePixel(x + col, y + row, std::byte(alpha));
                }
            }
        } else if (pBitmap->pixel_mode == FT_PIXEL_MODE_MONO) {
            for (FT_UInt row = 0; row < pBitmap->rows; row++) {
                for (FT_UInt col = 0; col < pBitmap->width; col++) {
                    FT_UInt index = row * pBitmap->pitch + col / 8;
                    FT_Byte alpha = (pBitmap->buffer[index] & (0x80 >> (col % 8))) ? 255 : 0;

                    writePixel(x + col, y + row, std::byte(alpha));
                }
            }
        } else {
            LOG_ASSERT("unsupported pixel mode `{}` (mode={})", pName, pBitmap->pixel_mode);
        }
    }

    struct FontRender {
        FontRender(FT_Face face, depot::CanvasPoint origin, depot::CanvasSize inSize, float deg, int pt)
            : face(face)
            , slot(face->glyph)
            , origin(origin)
            , size(inSize)
        {
            // TODO: figure this out a bit better
            if (size == size_t(0)) {
                size = {
                    core::intCast<FT_UInt>(face->size->metrics.max_advance >> 6),
                    core::intCast<FT_UInt>(face->size->metrics.height >> 6)
                };
            }

            setMatrixAngle(deg);
            setPen(pt);

            height = face->size->metrics.height;

            image = { size };
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
                const char *pError = FT_Error_String(error);
                LOG_ASSERT("failed to load glyph (codepoint={}, fterr={})", uint32_t(codepoint), pError == nullptr ? "unknown" : pError);
            }

            FT_Bitmap *bitmap = &slot->bitmap;
            bltGlyph(image, face->family_name, bitmap, slot->bitmap_left, core::intCast<FT_UInt>(size.height) - slot->bitmap_top, colour);
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

Font::Font(std::shared_ptr<IFile> pFile) {
    FT_Library library = FreeTypeService::getLibrary();
    auto memory = pFile->blob();

    const FT_Byte *pData = reinterpret_cast<FT_Byte *>(memory.data());
    FT_Long length = core::intCast<FT_Long>(memory.size());

    if (FT_Error error = FT_New_Memory_Face(library, pData, length, 0, &face)) {
        LOG_ASSERT("failed to load font face from `{}` (fterr={})", pFile->getName(), FT_Error_String(error));
    }

    if (FT_Error error = FT_Select_Charmap(face, FT_ENCODING_UNICODE)) {
        if (const char *pError = FT_Error_String(error)) {
            LOG_WARN("failed to select unicode charmap (fterr={})", pError);
        } else {
            LOG_WARN("failed to select unicode charmap (fterr={:#x})", error);
        }
    }
}

Font::Font(const fs::path& path) {
    FT_Library library = FreeTypeService::getLibrary();

    if (FT_Error error = FT_New_Face(library, path.string().c_str(), 0, &face)) {
        const char *pError = FT_Error_String(error);
        LOG_ASSERT("failed to load font face from `{}` (fterr={})", path.string(), pError == nullptr ? "unknown" : pError);
    }

    LOG_DEBUG("{} available font sizes for `{}`:", face->num_fixed_sizes, face->family_name);
    for (int i = 0; i < face->num_fixed_sizes; i++) {
        FT_Bitmap_Size size = face->available_sizes[i];
        LOG_DEBUG("  {}pt ({}x{})", size.height >> 6, size.width, size.height);
    }

    LOG_DEBUG("{} available font charmaps for `{}`:", face->num_charmaps, face->family_name);
    for (int i = 0; i < face->num_charmaps; i++) {
        FT_CharMap charmap = face->charmaps[i];
        char encoding[5] = { 0 };
        memcpy(encoding, &charmap->encoding, 4);
        LOG_DEBUG("  {} {}.{}", encoding, charmap->platform_id, charmap->encoding_id);
    }

    if (FT_Error error = FT_Select_Charmap(face, FT_ENCODING_UNICODE)) {
        if (const char *pError = FT_Error_String(error)) {
            LOG_WARN("failed to select unicode charmap `{}` (fterr={})", face->family_name, pError);
        } else {
            LOG_WARN("failed to select unicode charmap `{}` (fterr={:#x})", face->family_name, error);
        }
    }
}

Font::~Font() {
    FT_Done_Face(face);
}

Font::Font(Font&& other) noexcept
    : face(other.face)
    , pt(other.pt)
    , dpi(other.dpi)
{
    other.face = nullptr;
}

void Font::setFontSize(int newPt, int newDpi) {
    if (pt == newPt && dpi == newDpi)
        return;

    pt = newPt;
    dpi = newDpi;

    LOG_INFO("setting font `{}` size to {}pt (dpi={})", face->family_name, pt, dpi);
    if (FT_Error error = FT_Set_Char_Size(face, pt * 64, 0, dpi, 0)) {
        if (const char *pError = FT_Error_String(error)) {
            LOG_WARN("failed to set font size `{}` (fterr={})", face->family_name, pError);
        } else {
            LOG_WARN("failed to set font size `{}` (fterr={:#x})", face->family_name, error);
        }
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

CanvasSize Font::getGlyphSize(char32_t glyph) const {
    if (FT_Error error = FT_Load_Char(face, glyph, FT_LOAD_DEFAULT)) {
        const char *pError = FT_Error_String(error);
        LOG_ASSERT("failed to load glyph (codepoint={}, fterr={})", uint32_t(glyph), pError == nullptr ? "unknown" : pError);
    }

    return {
        core::intCast<FT_UInt>(face->glyph->metrics.width >> 6),
        core::intCast<FT_UInt>(face->glyph->metrics.height >> 6)
    };
}

void Font::drawGlyph(char32_t codepoint, CanvasPoint start, Image& image) {
    FT_Set_Transform(face, nullptr, nullptr);

    if (FT_Error error = FT_Load_Char(face, codepoint, FT_LOAD_RENDER)) {
        if (const char *pError = FT_Error_String(error)) {
            LOG_ASSERT("failed to load glyph (codepoint={}, fterr={})", uint32_t(codepoint), pError);
        } else { 
            LOG_ASSERT("failed to load glyph (codepoint={}, fterr={:#x})", uint32_t(codepoint), error);
        }
    }

    FT_Bitmap *bitmap = &face->glyph->bitmap;
    bltGlyph(image, face->family_name, bitmap, core::intCast<FT_UInt>(start.x), core::intCast<FT_UInt>(start.y), kBlack);
}
