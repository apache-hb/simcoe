#pragma once

#include "engine/render/render.h"
#include <unordered_map>

namespace simcoe::render {
    struct Graph;

    enum StateDep {
        eDepNone = 0,
        eDepDevice = (1 << 0),
        eDepDisplaySize = (1 << 1),
        eDepRenderSize = (1 << 2),
        eDepBackBufferCount = (1 << 3),
    };

    struct IGraphObject {
        virtual ~IGraphObject() = default;

        IGraphObject(Graph *pGraph, std::string name, StateDep stateDeps = eDepDevice);

        virtual void create() = 0;
        virtual void destroy() = 0;

        bool dependsOn(StateDep dep) const { return (stateDeps & dep) != 0; }
        std::string_view getName() const { return name; }

    protected:
        Graph *pGraph;
        Context *ctx;

    private:
        std::string name;
        StateDep stateDeps;
    };

    struct IResourceHandle : IGraphObject {
        virtual ~IResourceHandle() = default;
        IResourceHandle(Graph *pGraph, std::string name, StateDep stateDeps = eDepDevice)
            : IGraphObject(pGraph, name, stateDeps)
        { }

        virtual rhi::DeviceResource* getResource() const = 0;

        rhi::ResourceState getCurrentState() const;
        void setCurrentState(rhi::ResourceState state);

    protected:
        rhi::ResourceState getResourceState(rhi::DeviceResource *pResource) const;
        void setResourceState(rhi::DeviceResource *pResource, rhi::ResourceState state);
    };

    template<std::derived_from<rhi::DeviceResource> T>
    struct ISingleResourceHandle : IResourceHandle {
        virtual ~ISingleResourceHandle() = default;
        ISingleResourceHandle(Graph *pGraph, std::string name, StateDep stateDeps = eDepDevice)
            : IResourceHandle(pGraph, name, stateDeps)
        { }

        void destroy() override {
            delete pResource;
        }

        rhi::DeviceResource* getResource() const final override { return pResource; }

    protected:
        void setResource(T *pResource) {
            this->pResource = pResource;
            setResourceName(getName());
        }

        T *getBuffer() const { return pResource; }

    private:
        void setResourceName(std::string_view name) {
            pResource->setName(name);
        }
        T *pResource = nullptr;
    };

    ///
    /// rtv handles
    ///

    struct IRTVHandle {
        virtual ~IRTVHandle() = default;

        virtual RenderTargetAlloc::Index getRtvIndex() const = 0;

        math::float4 getClearColour() const { return clearColour; }

    protected:
        void setClearColour(math::float4 clearColour) { this->clearColour = clearColour; }

    private:
        math::float4 clearColour = { 0.0f, 0.0f, 0.0f, 1.0f };
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

    ///
    /// dsv handles
    ///

    struct IDSVHandle {
        virtual ~IDSVHandle() = default;

        virtual DepthStencilAlloc::Index getDsvIndex() const = 0;
    };

    struct ISingleDSVHandle : IDSVHandle {
        virtual ~ISingleDSVHandle() = default;

        DepthStencilAlloc::Index getDsvIndex() const final override {
            return dsvIndex;
        }
    protected:
        void destroy(Context *ctx) {
            auto *pDsvHeap = ctx->getDsvHeap();
            pDsvHeap->release(dsvIndex);
        }

        void setDsvIndex(DepthStencilAlloc::Index index) { dsvIndex = index; }
    private:
        DepthStencilAlloc::Index dsvIndex;
    };

    ///
    /// srv handles
    ///

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
    /// uav handles
    ///

    struct IUAVHandle {
        virtual ~IUAVHandle() = default;

        virtual ShaderResourceAlloc::Index getUavIndex() const = 0;
    };

    struct ISingleUAVHandle : IUAVHandle {
        virtual ~ISingleUAVHandle() = default;

        ShaderResourceAlloc::Index getUavIndex() const final override {
            return uavIndex;
        }

    protected:
        void destroy(Context *ctx) {
            auto *pUavHeap = ctx->getSrvHeap();
            pUavHeap->release(uavIndex);
        }

        void setUavIndex(ShaderResourceAlloc::Index index) { uavIndex = index; }
    private:
        ShaderResourceAlloc::Index uavIndex;
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
        template<typename O> requires std::derived_from<O, T> && std::derived_from<O, IResourceHandle>
        ResourceWrapper(O *pHandle)
            : BaseResourceWrapper(pHandle)
            , pHandle(pHandle)
        { }

        template<typename O> requires std::derived_from<T, O>
        ResourceWrapper<O> *as() const {
            return new ResourceWrapper<O>(pHandle);
        }

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

    struct ICommandPass : IGraphObject {
        virtual ~ICommandPass() = default;
        ICommandPass(Graph *pGraph, std::string name, StateDep stateDeps = eDepDevice)
            : IGraphObject(pGraph, name, stateDeps)
        { }

        virtual void executePass() { execute(); }

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

    struct IRenderPass : ICommandPass {
        virtual ~IRenderPass() = default;
        IRenderPass(Graph *pGraph, std::string name, StateDep stateDeps = eDepDevice)
            : ICommandPass(pGraph, name, stateDeps)
        { }

        void executePass() override;

        IRTVHandle *getRenderTarget() const { return pRenderTarget->getInner(); }
        IDSVHandle *getDepthStencil() const { return pDepthStencil->getInner(); }

    protected:
        void setRenderTargetHandle(ResourceWrapper<IRTVHandle> *pHandle) {
            this->pRenderTarget = addAttachment(pHandle, rhi::ResourceState::eRenderTarget);
        }

        void setDepthStencilHandle(ResourceWrapper<IDSVHandle> *pHandle) {
            this->pDepthStencil = addAttachment(pHandle, rhi::ResourceState::eDepthWrite);
        }

    private:
        PassAttachment<IRTVHandle> *pRenderTarget = nullptr;
        PassAttachment<IDSVHandle> *pDepthStencil = nullptr;
    };

    ///
    /// render graph
    ///

    struct Graph {
        Graph(Context *ctx)
            : ctx(ctx)
        { }

        ~Graph() {
            withLock([this]{
                destroyIf(eDepDevice); // everything depends on device
            });
        }

        // state management

        template<std::derived_from<ICommandPass> T, typename... A>
        T *addPass(A&&... args) {
            T *pPass = new T(this, std::forward<A>(args)...);
            addPassObject(pPass);
            return pPass;
        }

        template<std::derived_from<IResourceHandle> T, typename... A>
        ResourceWrapper<T> *addResource(A&&... args) {
            T *pHandle = new T(this, std::forward<A>(args)...);
            addResourceObject(pHandle);
            return new ResourceWrapper<T>(pHandle);
        }

        template<std::derived_from<IGraphObject> T, typename... A>
        T *newGraphObject(A&&... args) {
            T *pObject = new T(this, std::forward<A>(args)...);
            addGraphObject(pObject);
            return pObject;
        }

        void removePass(ICommandPass *pPass);
        void removeResource(IResourceHandle *pHandle);
        void removeObject(IGraphObject *pObject);

        // getters

        Context *getContext() const { return ctx; }
        const RenderCreateInfo &getCreateInfo() const { return ctx->getCreateInfo(); }

        // setters

        void setFullscreen(bool bFullscreen);
        void resizeDisplay(UINT width, UINT height);
        void resizeRender(UINT width, UINT height);
        void changeBackBufferCount(UINT count);
        void changeAdapter(UINT index);

        void resumeFromFault();

        ///
        /// pass execution
        ///

        bool execute();
        IRTVHandle *pCurrentRenderTarget = nullptr;

    private:
        void executePass(ICommandPass *pPass);

        ///
        /// state management
        ///

        void addResourceObject(IResourceHandle *pHandle);
        void addPassObject(ICommandPass *pPass);
        void addGraphObject(IGraphObject *pObject);

        template<typename F>
        void changeData(StateDep dep, F&& func) {
            withLock([=] {
                ctx->waitForDirectQueue();
                ctx->waitForCopyQueue();

                destroyIf(dep);
                func();
                createIf(dep);
            });
        }

        template<typename F>
        void withLock(F&& func) {
            lock = true;
            std::lock_guard guard(renderLock);

            func();

            lock = false;
        }

        void createIf(StateDep dep);
        void destroyIf(StateDep dep);

        std::atomic_bool lock;
        std::recursive_mutex renderLock;

    public:
        void setResourceState(rhi::DeviceResource *pResource, rhi::ResourceState state) {
            resourceStates[pResource] = state;
        }

        rhi::ResourceState getResourceState(rhi::DeviceResource *pResource) const {
            return resourceStates.at(pResource);
        }

    private:
        std::unordered_map<rhi::DeviceResource*, rhi::ResourceState> resourceStates;

        Context *ctx;

    public:
        // TODO: make private
        std::vector<ICommandPass*> passes;
        std::vector<IResourceHandle*> resources;
        std::vector<IGraphObject*> objects;
    };
}
