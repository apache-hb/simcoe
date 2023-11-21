#pragma once

#include "game/render/hud.h"

namespace game::ui {
    struct Context;

    struct BoxBounds {
        float2 min;
        float2 max;
    };

    struct UiVertex {
        float2 position;
        float2 uv;
        uint32_t colour;
    };

    enum struct AlignV {
        eTop, eMiddle, eBottom, eCount
    };

    enum struct AlignH {
        eLeft, eCenter, eRight, eCount
    };

    struct Align { 
        AlignV v = AlignV::eMiddle;
        AlignH h = AlignH::eCenter;
    };

    struct DrawData {
        std::vector<UiVertex> vertices;
        std::vector<uint16_t> indices;
    };

    struct DrawInfo {
        BoxBounds bounds;
        uint32_t colour;
    };

    struct IWidget {
        virtual ~IWidget() = default;

        virtual void draw(Context *pContext, const DrawInfo& info) const = 0;

        float2 minsize;
        float2 maxsize; // set to 0,0 for no max size

        Align align;

        std::vector<IWidget*> children;
    };

    struct FontAtlas final : editor::graph::ITextureHandle, ISingleSRVHandle {
        FontAtlas(Graph *ctx, const fs::path& path, size_t pt, std::span<char32_t> chars);

        void create() override;
        void destroy() override;

        BoxBounds getBounds(char32_t glyph) const;

    private:
        std::unordered_map<char32_t, BoxBounds> glyphs; // glyph to uv coord bounds

        depot::Font font;
        depot::Image bitmap;
    };

    struct TextWidget : IWidget {
        TextWidget(std::string text);

        void draw(Context *pContext, const DrawInfo& info) const override;

        std::string text;

        FontAtlas *pAtlas;
    };

    // core ui class
    // generates draw lists
    struct Context {
        Context(BoxBounds screen);

        const DrawData& getDrawData();

        void box(const BoxBounds& bounds, uint32_t colour);
        void letter(const BoxBounds& bounds, uint32_t colour, char32_t letter);

    private:
        BoxBounds screen; // the complete bounds of the screen (draws at display resolution)
        BoxBounds user; // the bounds of the user interface

        DrawData data;
    };
}
