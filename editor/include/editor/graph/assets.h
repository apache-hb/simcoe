#pragma once

#include "editor/render/graph.h"

namespace editor::graph {
    using ITextureHandle = ISingleResourceHandle<render::TextureBuffer>;
    using IUniformHandle = ISingleResourceHandle<render::UniformBuffer>;

    struct SwapChainHandle final : IResourceHandle {
        SwapChainHandle(RenderContext *ctx)
            : IResourceHandle(ctx, StateDep(eDepDisplaySize | eDepBackBufferCount))
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
            : ITextureHandle(ctx, StateDep(eDepRenderSize))
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
        void destroy() override;

        RenderTargetAlloc::Index getRtvIndex() const override {
            return RenderTargetAlloc::Index::eInvalid;
        }

    private:
        std::string name;
    };

}
