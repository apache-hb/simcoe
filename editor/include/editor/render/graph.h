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

        virtual void create() = 0;
        virtual void destroy() = 0;

        bool dependsOn(StateDep dep) const { return (stateDeps & dep) != 0; }

    protected:
        RenderContext *ctx;

    private:
        StateDep stateDeps;
    };

    struct IResourceHandle : IGraphObject {
        virtual ~IResourceHandle() = default;
        IResourceHandle(RenderContext *ctx, StateDep stateDeps = eDepDevice)
            : IGraphObject(ctx, stateDeps)
        { }

        virtual render::DeviceResource* getResource() const = 0;

        virtual render::ResourceState getCurrentState() const = 0;
        virtual void setCurrentState(render::ResourceState state) = 0;

        virtual RenderTargetAlloc::Index getRtvIndex() const { return RenderTargetAlloc::Index::eInvalid; }
        virtual ShaderResourceAlloc::Index getSrvIndex() const { return ShaderResourceAlloc::Index::eInvalid; }
    };

    template<typename T>
    struct IAnyResourceHandle : IResourceHandle {
        using IResourceHandle::IResourceHandle;
        virtual ~IAnyResourceHandle() = default;

        void destroy() override {
            delete pResource;
        }

        render::DeviceResource* getResource() const final override { return pResource; }
        render::ResourceState getCurrentState() const final override { return currentState; }
        void setCurrentState(render::ResourceState state) final override { currentState = state; }

    protected:
        T *getBuffer() const { return pResource; }
        void setResource(T *pResource) { this->pResource = pResource; }

    private:
        T *pResource;
        render::ResourceState currentState;
    };

    template<typename T>
    struct IShaderResourceHandle : IAnyResourceHandle<T> {
        using Super = IAnyResourceHandle<T>;
        using Super::Super;
        virtual ~IShaderResourceHandle() = default;

        void destroy() override {
            auto *pSrvHeap = this->ctx->getSrvHeap();
            pSrvHeap->release(getSrvIndex());

            Super::destroy();
        }

        ShaderResourceAlloc::Index getSrvIndex() const final override { return srvIndex; }

    protected:
        void setSrvIndex(ShaderResourceAlloc::Index index) { srvIndex = index; }

    private:
        ShaderResourceAlloc::Index srvIndex;
    };

    struct BasePassAttachment {
        virtual ~BasePassAttachment() = default;
        virtual IResourceHandle *getHandle() const = 0;

        BasePassAttachment(render::ResourceState requiredState)
            : requiredState(requiredState)
        { }

        render::ResourceState requiredState;
    };

    template<typename T>
    struct PassAttachment : BasePassAttachment {
        PassAttachment(T *pHandle, render::ResourceState requiredState)
            : BasePassAttachment(requiredState)
            , pHandle(pHandle)
        { }

        T *getHandle() const override { return pHandle; }

    private:
        T *pHandle = nullptr;
    };

    struct IRenderPass : IGraphObject {
        virtual ~IRenderPass() = default;
        IRenderPass(RenderContext *ctx, StateDep stateDeps = eDepDevice)
            : IGraphObject(ctx, stateDeps)
        { }

        virtual void execute() = 0;

        std::vector<BasePassAttachment*> inputs;

    protected:
        template<typename T>
        PassAttachment<T> *addAttachment(T *pHandle, render::ResourceState requiredState) {
            auto *pResource = new PassAttachment<T>(pHandle, requiredState);
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
