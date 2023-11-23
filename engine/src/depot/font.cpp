#include "engine/depot/font.h"

#include "engine/core/units.h"
#include "engine/service/freetype.h"
#include "engine/log/service.h"

#include "engine/math/math.h"

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

    void bltGlyph(Image& image, char32_t codepoint, FT_String *pName, FT_Bitmap *pBitmap, FT_UInt x, FT_UInt y, math::float4 colour) {
        auto writePixel = [&](size_t x, size_t y, uint8_t value, math::float4 col) {
            size_t index = (y * image.size.width + x) * 4;
            if (index + 3 > image.data.size()) return;

            image.data[index + 0] = std::byte(value * col.r);
            image.data[index + 1] = std::byte(value * col.g);
            image.data[index + 2] = std::byte(value * col.b);
            image.data[index + 3] = std::byte(value * col.a);
        };

        if (pBitmap->pixel_mode == FT_PIXEL_MODE_GRAY) {
            for (FT_UInt row = 0; row < pBitmap->rows; row++) {
                for (FT_UInt col = 0; col < pBitmap->width; col++) {
                    FT_UInt index = row * pBitmap->pitch + col;
                    FT_Byte alpha = pBitmap->buffer[index];

                    writePixel(x + col, y + row, uint8_t(alpha), colour);
                }
            }
        } else if (pBitmap->pixel_mode == FT_PIXEL_MODE_MONO) {
            for (FT_UInt row = 0; row < pBitmap->rows; row++) {
                for (FT_UInt col = 0; col < pBitmap->width; col++) {
                    FT_UInt index = row * pBitmap->pitch + col / 8;
                    FT_Byte alpha = (pBitmap->buffer[index] & (0x80 >> (col % 8))) ? 255 : 0;

                    writePixel(x + col, y + row, uint8_t(alpha), colour);
                }
            }
        } else {
            // fill the glyph with magenta
            for (FT_UInt row = 0; row < pBitmap->rows; row++) {
                for (FT_UInt col = 0; col < pBitmap->width; col++) {
                    writePixel(x + col, y + row, uint8_t(255), math::float4(1.f, 0.f, 1.f, 1.f));
                }
            }

            LOG_WARN("unsupported pixel mode `{:#x}` `{}` (mode={})", int32_t(codepoint), pName, pBitmap->pixel_mode);
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
            bltGlyph(image, codepoint, face->family_name, bitmap, slot->bitmap_left, core::intCast<FT_UInt>(size.height) - slot->bitmap_top, colour);
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
{
    other.face = nullptr;
}

void Font::setFontSize(int newPt, int hdpi, int vdpi) {
    if (pt == newPt)
        return;

    pt = newPt;

    LOG_INFO("setting font `{}` size to {}pt (dpi={}x{})", face->family_name, pt, hdpi, vdpi);
    if (FT_Error error = FT_Set_Char_Size(face, 0, pt * 64, hdpi, vdpi)) {
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

void Font::drawGlyph(char32_t codepoint, CanvasPoint start, Image& image, const math::float4& colour) {
    FT_Set_Transform(face, nullptr, nullptr);

    if (FT_Error error = FT_Load_Char(face, codepoint, FT_LOAD_RENDER)) {
        if (const char *pError = FT_Error_String(error)) {
            LOG_WARN("failed to load glyph (codepoint={}, fterr={})", uint32_t(codepoint), pError);
        } else { 
            LOG_WARN("failed to load glyph (codepoint={}, fterr={:#x})", uint32_t(codepoint), error);
        }

        return;
    }

    FT_Bitmap *bitmap = &face->glyph->bitmap;
    bltGlyph(image, codepoint, face->family_name, bitmap, core::intCast<FT_UInt>(start.x), core::intCast<FT_UInt>(start.y), colour);
}

// harfbuzz

ShapedTextIterator::ShapedTextIterator(unsigned int index, hb_glyph_info_t *pGlyphInfo, hb_glyph_position_t *pGlyphPos)
    : index(index)
    , pGlyphInfo(pGlyphInfo)
    , pGlyphPos(pGlyphPos)
{ }

bool ShapedTextIterator::operator==(const ShapedTextIterator& other) const {
    return index == other.index;
}

bool ShapedTextIterator::operator!=(const ShapedTextIterator& other) const {
    return index != other.index;
}

ShapedGlyph ShapedTextIterator::operator*() const {
    return {
        .codepoint = pGlyphInfo[index].codepoint,
        .xAdvance = pGlyphPos[index].x_advance >> 6,
        .yAdvance = pGlyphPos[index].y_advance >> 6,
        .xOffset = pGlyphPos[index].x_offset >> 6,
        .yOffset = pGlyphPos[index].y_offset >> 6
    };
}

ShapedTextIterator& ShapedTextIterator::operator++() {
    index++;
    return *this;
}

ShapedText::ShapedText(hb_buffer_t *pBuffer) : pBuffer(pBuffer) { 
    numGlyphs = hb_buffer_get_length(pBuffer);
    pGlyphInfo = hb_buffer_get_glyph_infos(pBuffer, nullptr);
    pGlyphPos = hb_buffer_get_glyph_positions(pBuffer, nullptr);
}

ShapedText::ShapedText(ShapedText&& other) noexcept
    : pBuffer(other.pBuffer)
    , numGlyphs(other.numGlyphs)
    , pGlyphInfo(other.pGlyphInfo)
    , pGlyphPos(other.pGlyphPos)
{
    other.pBuffer = nullptr;
    other.numGlyphs = 0;
    other.pGlyphInfo = nullptr;
    other.pGlyphPos = nullptr;
}

ShapedText& ShapedText::operator=(ShapedText&& other) noexcept {
    if (this != &other) {
        if (pBuffer) hb_buffer_destroy(pBuffer);

        pBuffer = other.pBuffer;
        numGlyphs = other.numGlyphs;
        pGlyphInfo = other.pGlyphInfo;
        pGlyphPos = other.pGlyphPos;

        other.pBuffer = nullptr;
        other.numGlyphs = 0;
        other.pGlyphInfo = nullptr;
        other.pGlyphPos = nullptr;
    }

    return *this;
}

ShapedText::~ShapedText() {
    if (pBuffer) hb_buffer_destroy(pBuffer);
}

ShapedTextIterator ShapedText::begin() const {
    return { 0, pGlyphInfo, pGlyphPos };
}

ShapedTextIterator ShapedText::end() const {
    return { numGlyphs, pGlyphInfo, pGlyphPos };
}

Text::Text(Font *pFreeTypeFont) {
    hb_font_t *pNewFont = hb_ft_font_create_referenced(pFreeTypeFont->getFace());
    hb_face_t *pNewFace = hb_font_get_face(pNewFont);

    hb_ft_font_set_funcs(pNewFont);
    hb_face_set_upem(pNewFace, 64);

    pFont = pNewFont;
    pFace = pNewFace;

    int x, y;
    hb_font_get_scale(pFont, &x, &y);

    LOG_INFO("scale: {}x{}", x, y);
}

Text::~Text() {
    if (pFont) hb_font_destroy(pFont);
}

ShapedText Text::shape(utf8::StaticText text) {
    hb_buffer_t *pBuffer = hb_buffer_create();
    hb_buffer_add_utf8(pBuffer, (const char*)text.data(), int(text.size()), 0, int(text.size()));
    
    hb_buffer_set_direction(pBuffer, HB_DIRECTION_LTR);
    hb_buffer_set_script(pBuffer, HB_SCRIPT_LATIN);
    hb_buffer_set_language(pBuffer, hb_language_from_string("en", -1));
    //hb_buffer_guess_segment_properties(pBuffer);

    hb_shape(pFont, pBuffer, nullptr, 0);

    return ShapedText(pBuffer);
}
