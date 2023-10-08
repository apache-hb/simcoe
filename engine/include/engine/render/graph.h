#pragma once

#include "engine/render/render.h"

namespace simcoe::render {
    struct Graph;

    enum StateDep {
        eNone = 0,
        eDepDevice = (1 << 0),
        eDepDisplaySize = (1 << 1),
        eDepRenderSize = (1 << 2),
        eDepBackBufferCount = (1 << 3),
    };

    struct IGraphObject {
        virtual ~IGraphObject() = default;

        IGraphObject(Graph *graph, std::string name, StateDep stateDeps = eDepDevice);

        virtual void create() = 0;
        virtual void destroy() = 0;

        bool dependsOn(StateDep dep) const { return (stateDeps & dep) != 0; }
        std::string_view getName() const { return name; }

    protected:
        Graph *graph;
        Context *ctx;

    private:
        std::string name;
        StateDep stateDeps;
    };

    struct IResourceHandle : IGraphObject {
        virtual ~IResourceHandle() = default;
        IResourceHandle(Graph *ctx, std::string name, StateDep stateDeps = eDepDevice)
            : IGraphObject(ctx, name, stateDeps)
        { }

        virtual rhi::DeviceResource* getResource() const = 0;

        virtual rhi::ResourceState getCurrentState() const = 0;
        virtual void setCurrentState(rhi::ResourceState state) = 0;
    };

    template<typename T>
    struct ISingleResourceHandle : IResourceHandle {
        static_assert(std::is_base_of_v<rhi::DeviceResource, T>);

        virtual ~ISingleResourceHandle() = default;
        ISingleResourceHandle(Graph *ctx, std::string name, StateDep stateDeps = eDepDevice)
            : IResourceHandle(ctx, name, stateDeps)
        { }

        void destroy() override {
            delete pResource;
        }

        rhi::ResourceState getCurrentState() const final override { return currentState; }
        void setCurrentState(rhi::ResourceState state) final override { currentState = state; }

        rhi::DeviceResource* getResource() const final override { return pResource; }

    protected:
        void setResource(T *pResource) { this->pResource = pResource; }
        T *getBuffer() const { return pResource; }

    private:
        T *pResource = nullptr;
        rhi::ResourceState currentState;
    };

    struct IRTVHandle {
        virtual ~IRTVHandle() = default;

        virtual RenderTargetAlloc::Index getRtvIndex() const = 0;
    };

    struct ISingleRTVHandle : IRTVHandle {
        virtual ~ISingleRTVHandle() = default;

        RenderTargetAlloc::Index getRtvIndex() const final override {
            return rtvIndex;
        }

    protected:
        void destroy(Context *ctx) {
            auto *pRtvHeap = ctx->getRtvHeap();
            pRtvHeap->release(rtvIndex);
        }

        void setRtvIndex(RenderTargetAlloc::Index index) { rtvIndex = index; }

    private:
        RenderTargetAlloc::Index rtvIndex;
    };

    struct ISRVHandle {
        virtual ~ISRVHandle() = default;

        virtual ShaderResourceAlloc::Index getSrvIndex() const = 0;
    };

    struct ISingleSRVHandle : ISRVHandle {
        virtual ~ISingleSRVHandle() = default;

        ShaderResourceAlloc::Index getSrvIndex() const final override {
            return srvIndex;
        }

    protected:
        void destroy(Context *ctx) {
            auto *pSrvHeap = ctx->getSrvHeap();
            pSrvHeap->release(srvIndex);
        }

        void setSrvIndex(ShaderResourceAlloc::Index index) { srvIndex = index; }

    private:
        ShaderResourceAlloc::Index srvIndex;
    };

    ///
    /// resources allocated by the graph
    ///

    struct BaseResourceWrapper {
        BaseResourceWrapper(IResourceHandle *pResource)
            : pResource(pResource)
        { }

        IResourceHandle *getHandle() const { return pResource; }

    private:
        IResourceHandle *pResource = nullptr;
    };

    template<typename T>
    struct ResourceWrapper final : BaseResourceWrapper {
        template<typename O>
        ResourceWrapper(O *pHandle)
            : BaseResourceWrapper(pHandle)
            , pHandle(pHandle)
        { }

        template<typename O>
        ResourceWrapper<O> *as() const { return new ResourceWrapper<O>(pHandle); }

        T *getInner() const { return static_cast<T*>(pHandle); }
    private:
        T *pHandle = nullptr;
    };


    ///
    /// render pass attachments
    ///

    struct BasePassAttachment {
        BasePassAttachment(rhi::ResourceState requiredState)
            : requiredState(requiredState)
        { }

        virtual IResourceHandle *getResourceHandle() const = 0;
        rhi::ResourceState getRequiredState() const { return requiredState; }

    private:
        rhi::ResourceState requiredState;
    };

    template<typename T>
    struct PassAttachment final : BasePassAttachment {
        PassAttachment(ResourceWrapper<T> *pWrap, rhi::ResourceState requiredState)
            : BasePassAttachment(requiredState)
            , pWrap(pWrap)
        { }

        IResourceHandle *getResourceHandle() const override { return pWrap->getHandle(); }
        T *getInner() const { return pWrap->getInner(); }

    private:
        ResourceWrapper<T> *pWrap = nullptr;
    };

    ///
    /// render passes
    ///

    struct IRenderPass : IGraphObject {
        virtual ~IRenderPass() = default;
        IRenderPass(Graph *ctx, std::string name, StateDep stateDeps = eDepDevice)
            : IGraphObject(ctx, name, stateDeps)
        { }

        virtual void execute() = 0;

        std::vector<BasePassAttachment*> inputs;

    protected:
        template<typename T>
        PassAttachment<T> *addAttachment(ResourceWrapper<T> *pHandle, rhi::ResourceState requiredState) {
            auto *pResource = new PassAttachment<T>(pHandle, requiredState);
            inputs.push_back(pResource);
            return pResource;
        }
    };

    ///
    /// render graph
    ///

    struct Graph {
        Graph(Context *ctx)
            : ctx(ctx)
        { }

        ~Graph() {
            destroyIf(eDepDevice); // everything depends on device
        }

        template<typename T, typename... A>
        T *addPass(A&&... args) {
            static_assert(std::is_base_of_v<IRenderPass, T>);

            T *pPass = new T(this, std::forward<A>(args)...);
            addPassObject(pPass);
            return pPass;
        }

        template<typename T, typename... A>
        ResourceWrapper<T> *addResource(A&&... args) {
            static_assert(std::is_base_of_v<IResourceHandle, T>);

            T *pHandle = new T(this, std::forward<A>(args)...);
            addResourceObject(pHandle);
            return new ResourceWrapper<T>(pHandle);
        }

        template<typename T, typename... A>
        T *addObject(A&&... args) {
            static_assert(std::is_base_of_v<IGraphObject, T>);

            T *pObject = new T(this, std::forward<A>(args)...);
            addGraphObject(pObject);
            return pObject;
        }

        void setFullscreen(bool bFullscreen);
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

        void addGraphObject(IGraphObject *pObject) {
            pObject->create();
            objects.push_back(pObject);
        }

        void executePass(IRenderPass *pPass);

        template<typename F>
        void changeData(StateDep dep, F&& func) {
            lock = true;
            std::lock_guard guard(renderLock);

            ctx->waitForDirectQueue();
            ctx->waitForCopyQueue();

            destroyIf(dep);
            func();
            createIf(dep);

            lock = false;
        }

        void createIf(StateDep dep);
        void destroyIf(StateDep dep);

        std::atomic_bool lock;
        std::mutex renderLock;

    public:
        Context *ctx;

        // TODO: make private
        std::vector<IRenderPass*> passes;
        std::vector<IResourceHandle*> resources;
        std::vector<IGraphObject*> objects;
    };
}
