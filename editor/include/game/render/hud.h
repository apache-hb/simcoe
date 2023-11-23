#pragma once

#include "engine/render/graph.h"

#include "editor/graph/assets.h"
#include "editor/ui/ui.h"

#include "game/render/hud/layout.h"
#include "game/render/scene.h"

#define SM_XB_LOGO "\uE001"
#define SM_XB_VIEW "\uE002"
#define SM_XB_MENU "\uE003"

#define SM_PLAYER_ICON "\uE004"
#define SM_NAME_UNDERLINE "\uE005"

namespace game::render {
    struct UiVertexBufferHandle final : ISingleResourceHandle<rhi::VertexBuffer> {
        using Super = ISingleResourceHandle<rhi::VertexBuffer>;
        UiVertexBufferHandle(Graph *pGraph, size_t size);

        void write(const std::vector<ui::UiVertex>& vertices);

        void create() override;

    private:
        size_t size;
        std::vector<ui::UiVertex> data;
    };

    struct UiIndexBufferHandle final : ISingleResourceHandle<rhi::IndexBuffer> {
        using Super = ISingleResourceHandle<rhi::IndexBuffer>;
        UiIndexBufferHandle(Graph *pGraph, size_t size);

        void write(const std::vector<ui::UiIndex>& indices);

        void create() override;

    private:
        size_t size;
        std::vector<ui::UiIndex> data;
    };

    struct FontAtlasInfo {
        fs::path path;
        size_t pt;
        std::vector<char32_t> glyphs;
    };

    struct FontAtlasHandle final : ITextureHandle, ISingleSRVHandle {
        FontAtlasHandle(Graph *pGraph, std::span<FontAtlasInfo> fonts);

        void create() override;
        void destroy() override;

        void draw();

        editor::ui::GlobalHandle debug = editor::ui::addGlobalHandle("font atlas", [this] {
            draw();
        });

        depot::Text getTextShaper(size_t idx);

        const ui::FontAtlasLookup& getAtlas() const { return glyphs; }

    private:
        std::vector<depot::Font> fonts;

        depot::Image bitmap;

        // map of codepoint -> uv bounds
        ui::FontAtlasLookup glyphs;
    };

    struct HudPass final : IRenderPass {
        HudPass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget);

        void create() override;
        void destroy() override;

        void execute() override;

        void update(const ui::Context& layout);

    private:
        mt::Mutex lock{"hud"};
        std::atomic_bool bDirty = true;
        std::vector<ui::UiVertex> vertices;
        std::vector<ui::UiIndex> indices;

        ResourceWrapper<UiVertexBufferHandle> *pVertexBuffer;
        ResourceWrapper<UiIndexBufferHandle> *pIndexBuffer;

    public:
        ResourceWrapper<FontAtlasHandle> *pFontAtlas;
        ResourceWrapper<ModelUniform> *pMatrix;

        rhi::PipelineState *pPipeline;
    };
}
