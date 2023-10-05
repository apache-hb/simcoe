#pragma once

#include "editor/render/render.h"

namespace editor {
    enum StateDep {
        eDepDevice = (1 << 0),
        eDepDisplaySize = (1 << 1),
        eDepRenderSize = (1 << 2),
        eDepBackBufferCount = (1 << 3),
    };

    struct IResourceHandle {
        virtual ~IResourceHandle() = default;
        IResourceHandle(StateDep stateDeps = eDepDevice)
            : stateDeps(StateDep(stateDeps | eDepDevice))
        { }

        virtual void create(RenderContext *ctx) = 0;
        virtual void destroy(RenderContext *ctx) = 0;

        virtual render::DeviceResource* getResource(RenderContext *ctx) const = 0;

        virtual render::ResourceState getCurrentState(RenderContext *ctx) const = 0;
        virtual void setCurrentState(RenderContext *ctx, render::ResourceState state) = 0;

        virtual RenderTargetAlloc::Index getRtvIndex(RenderContext *ctx) const = 0;

        ShaderResourceAlloc::Index srvIndex;
        StateDep stateDeps;
    };

    template<typename T>
    struct ISingleResourceHandle : IResourceHandle {
        using IResourceHandle::IResourceHandle;

        virtual ~ISingleResourceHandle() = default;

        render::DeviceResource* getResource(RenderContext *ctx) const final override { return pResource; }
        render::ResourceState getCurrentState(RenderContext *ctx) const final override { return currentState; }
        void setCurrentState(RenderContext *ctx, render::ResourceState state) final override { currentState = state; }

    protected:
        T *pResource;
        render::ResourceState currentState;
    };

    struct BasePassResource {
        virtual ~BasePassResource() = default;
        virtual IResourceHandle *getHandle() const = 0;

        BasePassResource(render::ResourceState requiredState)
            : requiredState(requiredState)
        { }

        render::ResourceState requiredState;
    };

    template<typename T>
    struct PassResource : BasePassResource {
        PassResource(T *pHandle, render::ResourceState requiredState)
            : BasePassResource(requiredState)
            , pHandle(pHandle)
        { }

        T *getHandle() const override { return pHandle; }
        T *pHandle = nullptr;
    };

    struct IRenderPass {
        virtual ~IRenderPass() = default;
        IRenderPass(StateDep stateDeps = eDepDevice)
            : stateDeps(StateDep(stateDeps | eDepDevice))
        { }

        virtual void create(RenderContext *ctx) = 0;
        virtual void destroy(RenderContext *ctx) = 0;

        virtual void execute(RenderContext *ctx) = 0;

        template<typename T>
        PassResource<T> *addResource(T *pHandle, render::ResourceState requiredState) {
            auto *pResource = new PassResource<T>(pHandle, requiredState);
            inputs.push_back(pResource);
            return pResource;
        }

        std::vector<BasePassResource*> inputs;
        StateDep stateDeps;
    };

    struct RenderGraph {
        RenderGraph(RenderContext *ctx)
            : ctx(ctx)
        { }

        ~RenderGraph() {
            destroyIf(eDepDevice); // everything depends on device
        }

        void addPass(IRenderPass *pPass) {
            pPass->create(ctx);
            passes.push_back(pPass);
        }

        void addResource(IResourceHandle *pHandle) {
            pHandle->create(ctx);

            resources.push_back(pHandle);
        }

        void resizeDisplay(UINT width, UINT height);
        void resizeRender(UINT width, UINT height);
        void changeBackBufferCount(UINT count);
        void changeAdapter(UINT index);

        void execute();
    private:
        void executePass(IRenderPass *pPass);

        void createIf(StateDep dep);
        void destroyIf(StateDep dep);

        std::atomic_bool lock;
        std::mutex renderLock;

        RenderContext *ctx;
        std::vector<IRenderPass*> passes;
        std::vector<IResourceHandle*> resources;
    };
}
