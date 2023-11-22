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

        virtual void draw(Context *pContext, const DrawInfo& info) const = 0;

        float2 minsize;
        float2 maxsize; // set to 0,0 for no max size

        Align align;

        std::vector<IWidget*> children;
    };

    struct TextWidget : IWidget {
        TextWidget(utf8::StaticText text);

        template<typename... A>
        void setTextUtf8(utf8::StaticText msg, A&&... args) {
            text = fmt::format(msg, std::forward<A>(args)...);
        }

        void draw(Context *pContext, const DrawInfo& info) const override;

        utf8::StaticText text;
    };

    struct TextDrawInfo {
        utf8::StaticText text;
        size_t size;
        Align align;
    };

    // core ui class
    // generates draw lists
    struct Context {
        Context(BoxBounds screen, size_t dpi);

        void begin();

        void box(const BoxBounds& bounds, uint8x4 colour);
        void text(const BoxBounds& bounds, uint8x4 colour, const TextDrawInfo& info);

        depot::Font *getFont(size_t size);

        float4x4 getMatrix() const;

    private:
        BoxBounds screen; // the complete bounds of the screen (draws at display resolution)
        BoxBounds user; // the bounds of the user interface

        size_t dpi;

    public:
        std::vector<UiVertex> vertices;
        std::vector<UiIndex> indices;

        // map of size -> font, we only use one font currently
        std::unordered_map<size_t, depot::Font> fonts;
    };
}
