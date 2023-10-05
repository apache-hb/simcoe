#pragma once

#include "editor/render/render.h"

namespace editor {
    enum StateDep {
        eDepDevice = (1 << 0),
        eDepDisplaySize = (1 << 1),
        eDepRenderSize = (1 << 2),
        eDepBackBufferCount = (1 << 3),
    };

    struct IGraphObject {
        virtual ~IGraphObject() = default;

        IGraphObject(RenderContext *ctx, StateDep stateDeps = eDepDevice)
            : ctx(ctx)
            , stateDeps(StateDep(stateDeps | eDepDevice))
        { }

        RenderContext *ctx;

        bool dependsOn(StateDep dep) const { return (stateDeps & dep) != 0; }
    private:
        StateDep stateDeps;
    };

    struct IResourceHandle : IGraphObject {
        virtual ~IResourceHandle() = default;
        IResourceHandle(RenderContext *ctx, StateDep stateDeps = eDepDevice)
            : IGraphObject(ctx, stateDeps)
        { }

        virtual void create() = 0;
        virtual void destroy() = 0;

        virtual render::DeviceResource* getResource() const = 0;

        virtual render::ResourceState getCurrentState() const = 0;
        virtual void setCurrentState(render::ResourceState state) = 0;

        virtual RenderTargetAlloc::Index getRtvIndex() const = 0;

        ShaderResourceAlloc::Index srvIndex;
    };

    template<typename T>
    struct ISingleResourceHandle : IResourceHandle {
        using IResourceHandle::IResourceHandle;

        virtual ~ISingleResourceHandle() = default;

        render::DeviceResource* getResource() const final override { return pResource; }
        render::ResourceState getCurrentState() const final override { return currentState; }
        void setCurrentState(render::ResourceState state) final override { currentState = state; }

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

    struct IRenderPass : IGraphObject {
        virtual ~IRenderPass() = default;
        IRenderPass(RenderContext *ctx, StateDep stateDeps = eDepDevice)
            : IGraphObject(ctx, stateDeps)
        { }

        virtual void create() = 0;
        virtual void destroy() = 0;

        virtual void execute() = 0;

        std::vector<BasePassResource*> inputs;

    protected:
        template<typename T>
        PassResource<T> *addResource(T *pHandle, render::ResourceState requiredState) {
            auto *pResource = new PassResource<T>(pHandle, requiredState);
            inputs.push_back(pResource);
            return pResource;
        }
    };

    struct RenderGraph {
        RenderGraph(RenderContext *ctx)
            : ctx(ctx)
        { }

        ~RenderGraph() {
            destroyIf(eDepDevice); // everything depends on device
        }

        template<typename T, typename... A>
        T *addPass(A&&... args) {
            T *pPass = new T(ctx, std::forward<A>(args)...);
            addPassObject(pPass);
            return pPass;
        }

        template<typename T, typename... A>
        T *addResource(A&&... args) {
            T *pHandle = new T(ctx, std::forward<A>(args)...);
            addResourceObject(pHandle);
            return pHandle;
        }

        void resizeDisplay(UINT width, UINT height);
        void resizeRender(UINT width, UINT height);
        void changeBackBufferCount(UINT count);
        void changeAdapter(UINT index);

        void execute();
    private:
        void addResourceObject(IResourceHandle *pHandle) {
            pHandle->create();
            resources.push_back(pHandle);
        }

        void addPassObject(IRenderPass *pPass) {
            pPass->create();
            passes.push_back(pPass);
        }

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
