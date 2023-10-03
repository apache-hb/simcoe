#pragma once

#include "editor/render/graph.h"

namespace editor::graph {
    using ITextureHandle = ISingleResourceHandle<render::TextureBuffer>;

    struct SwapChainHandle final : IResourceHandle {
        void create(RenderContext *ctx) override;
        void destroy(RenderContext *ctx) override;

        render::ResourceState getCurrentState(RenderContext *ctx) const override;
        void setCurrentState(RenderContext *ctx, render::ResourceState state) override;
        render::DeviceResource *getResource(RenderContext *ctx) const override;
        RenderTargetAlloc::Index getRtvIndex(RenderContext *ctx) const override;

    private:
        struct RenderTarget {
            render::RenderTarget *pRenderTarget;
            RenderTargetAlloc::Index rtvIndex;
            render::ResourceState state;
        };

        RenderTarget targets[RenderContext::kBackBufferCount];
    };

    struct SceneTargetHandle final : ITextureHandle {
        static constexpr math::float4 kClearColour = { 0.0f, 0.2f, 0.4f, 1.0f };

        void create(RenderContext *ctx) override;
        void destroy(RenderContext *ctx) override;

        RenderTargetAlloc::Index getRtvIndex(RenderContext *ctx) const override {
            return rtvIndex;
        }

    private:
        RenderTargetAlloc::Index rtvIndex;
    };

    struct TextureHandle final : ITextureHandle {
        TextureHandle(std::string name);

        void create(RenderContext *ctx) override;
        void destroy(RenderContext *ctx) override;

        RenderTargetAlloc::Index getRtvIndex(RenderContext *ctx) const override {
            return RenderTargetAlloc::Index::eInvalid;
        }

    private:
        std::string name;
    };

}
