#pragma once

#include "editor/render/graph.h"

namespace editor::graph {
    using ITextureHandle = IShaderResourceHandle<render::TextureBuffer>;
    using IUniformHandle = IShaderResourceHandle<render::UniformBuffer>;

    struct SwapChainHandle final : IResourceHandle {
        SwapChainHandle(RenderContext *ctx)
            : IResourceHandle(ctx, "swapchain.rtv", StateDep(eDepDisplaySize | eDepBackBufferCount))
        { }

        void create() override;
        void destroy() override;

        render::ResourceState getCurrentState() const override;
        void setCurrentState(render::ResourceState state) override;
        render::DeviceResource *getResource() const override;
        RenderTargetAlloc::Index getRtvIndex() const override;

    private:
        struct RenderTarget {
            render::RenderTarget *pRenderTarget;
            RenderTargetAlloc::Index rtvIndex;
            render::ResourceState state;
        };

        std::vector<RenderTarget> targets;
    };

    struct SceneTargetHandle final : ITextureHandle {
        SceneTargetHandle(RenderContext *ctx)
            : ITextureHandle(ctx, "texture.rtv", eDepRenderSize)
        { }

        static constexpr math::float4 kClearColour = { 0.0f, 0.2f, 0.4f, 1.0f };

        void create() override;
        void destroy() override;

        RenderTargetAlloc::Index getRtvIndex() const override {
            return rtvIndex;
        }

    private:
        RenderTargetAlloc::Index rtvIndex;
    };

    struct TextureHandle final : ITextureHandle {
        TextureHandle(RenderContext *ctx, std::string name);

        void create() override;

    private:
        std::string name;
    };
}
