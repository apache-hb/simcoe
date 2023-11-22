#pragma once

#include "engine/depot/image.h"

#include "engine/service/freetype.h"

#include "engine/core/utf8.h"

#include "engine/math/math.h"

#include <hb.h>

#include <span>

namespace simcoe::depot {
    using CanvasPoint = math::size2;
    using CanvasSize = math::size2;

    struct TextSegment {
        utf8::StaticText text;
        math::float4 colour = math::float4(1.f);
    };

    struct Glyph {
        CanvasPoint min;
        CanvasPoint max;
    };

    struct FontAtlas {
        Image image;

        std::unordered_map<char32_t, Glyph> glyphs;
    };

    struct Font {
        SM_NOCOPY(Font)

        SM_DEPRECATED("this is broken for some reason, use the filepath constructor for now")
        Font(std::shared_ptr<IFile> pFile);

        Font(const fs::path& path);
        ~Font();

        Font(Font&& other) noexcept;

        void setFontSize(int pt, int dpi);

        Image drawText(utf8::StaticText text, CanvasPoint start, CanvasSize size, float angle = 0.f);
        Image drawText(std::span<const TextSegment> segments, CanvasPoint start, CanvasSize size, float angle = 0.f);

        CanvasSize getGlyphSize(char32_t codepoint) const;
        void drawGlyph(char32_t codepoint, CanvasPoint start, Image& image);

        FT_Face getFace() const { return face; }

    private:
        FT_Face face;

        int pt;
        int dpi;
    };

    struct ShapedGlyph {
        hb_codepoint_t codepoint;
        hb_position_t xAdvance;
        hb_position_t yAdvance;
        hb_position_t xOffset;
        hb_position_t yOffset;
    };

    struct ShapedTextIterator {
        ShapedTextIterator(unsigned int index, hb_glyph_info_t *pGlyphInfo, hb_glyph_position_t *pGlyphPos);

        bool operator==(const ShapedTextIterator& other) const;
        bool operator!=(const ShapedTextIterator& other) const;

        ShapedGlyph operator*() const;
        ShapedTextIterator& operator++();

    private:
        unsigned int index;
        hb_glyph_info_t *pGlyphInfo;
        hb_glyph_position_t *pGlyphPos;
    };

    struct ShapedText {
        SM_NOCOPY(ShapedText)

        ShapedText(hb_buffer_t *pBuffer);
        ~ShapedText();

        ShapedTextIterator begin() const;
        ShapedTextIterator end() const;

    private:
        hb_buffer_t *pBuffer;

        unsigned int numGlyphs;
        hb_glyph_info_t *pGlyphInfo;
        hb_glyph_position_t *pGlyphPos;
    };

    struct Text {
        SM_NOCOPY(Text)

        Text(Font *pFreeTypeFont);
        ~Text();

        ShapedText shape(utf8::StaticText text);

    private:
        hb_font_t *pFont;
        hb_face_t *pFace;
    };
}
