#pragma once

#include "engine/math/math.h"

#include "engine/depot/font.h"

#include <vector>

namespace game::ui {
    namespace depot = simcoe::depot;
    namespace utf8 = simcoe::utf8;
    using namespace simcoe::math;

    using uint8x4 = Vec4<uint8_t>;
    using UiIndex = uint16_t;

    struct Context;

    struct BoxBounds {
        float2 min;
        float2 max;
    };

    struct FontGlyph {
        BoxBounds uvBounds;
        uint2 size;
    };

    using FontAtlasLookup = std::unordered_map<char32_t, FontGlyph>;

    struct UiVertex {
        float2 position;
        float2 uv;
        uint8x4 colour;
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

    struct DrawInfo {
        BoxBounds bounds;
        uint8x4 colour;
    };

    struct IWidget {
        virtual ~IWidget() = default;

        virtual BoxBounds draw(Context *pContext, const DrawInfo& info) const = 0;

        float2 minsize;
        float2 maxsize; // set to 0,0 for no max size

        Align align;
    };

    struct TextDrawInfo {
        utf8::StaticText text;
        Align align;
        float scale = 1.f;
        size_t shaper = 0;
    };

    struct TextWidget final : IWidget {
        TextWidget(utf8::StaticText text)
            : text(text)
        { }

        BoxBounds draw(Context *pContext, const DrawInfo& info) const override;

        float scale = 3.f;
        bool bDrawBox = true;
        size_t shaper = 0;

        utf8::StaticText text;
    };

    struct ButtonWidget final : IWidget {
        ButtonWidget(IWidget *pWidget)
            : pWidget(pWidget)
        { }

        BoxBounds draw(Context *pContext, const DrawInfo& info) const override;

        IWidget *pWidget;
    };

    struct HStackWidget final : IWidget {
        BoxBounds draw(Context *pContext, const DrawInfo& info) const override;

        void add(IWidget *pWidget) { children.push_back(pWidget); }

        std::vector<IWidget*> children;
    };

    struct VStackWidget final : IWidget {
        BoxBounds draw(Context *pContext, const DrawInfo& info) const override;

        void add(IWidget *pWidget) { children.push_back(pWidget); }

        std::vector<IWidget*> children;
    };

    // core ui class
    // generates draw lists
    struct Context {
        Context(BoxBounds screen);

        void begin(IWidget *pWidget);

        void box(const BoxBounds& bounds, uint8x4 colour);

        /// @returns the bounds of the text
        BoxBounds text(const BoxBounds& bounds, uint8x4 colour, const TextDrawInfo& info);

        float4x4 getMatrix() const;

        BoxBounds screen; // the complete bounds of the screen (draws at display resolution)
        BoxBounds user; // the bounds of the user interface

        std::vector<UiVertex> vertices;
        std::vector<UiIndex> indices;

        FontAtlasLookup atlas;
        std::vector<depot::Text> shapers;
    };
}
