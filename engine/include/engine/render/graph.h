#pragma once

#include "engine/render/render.h"

namespace simcoe::render {
    enum StateDep {
        eNone = 0,
        eDepDevice = (1 << 0),
        eDepDisplaySize = (1 << 1),
        eDepRenderSize = (1 << 2),
        eDepBackBufferCount = (1 << 3),
    };

    struct IGraphObject {
        virtual ~IGraphObject() = default;

        IGraphObject(Context *ctx, std::string name, StateDep stateDeps = eDepDevice)
            : ctx(ctx)
            , name(name)
            , stateDeps(StateDep(stateDeps | eDepDevice))
        { }

        virtual void create() = 0;
        virtual void destroy() = 0;

        bool dependsOn(StateDep dep) const { return (stateDeps & dep) != 0; }
        std::string_view getName() const { return name; }

    protected:
        Context *ctx;

    private:
        std::string name;
        StateDep stateDeps;
    };

    struct IResourceHandle : IGraphObject {
        virtual ~IResourceHandle() = default;
        IResourceHandle(Context *ctx, std::string name, StateDep stateDeps = eDepDevice)
            : IGraphObject(ctx, name, stateDeps)
        { }

        virtual rhi::DeviceResource* getResource() const = 0;

        virtual rhi::ResourceState getCurrentState() const = 0;
        virtual void setCurrentState(rhi::ResourceState state) = 0;

        virtual RenderTargetAlloc::Index getRtvIndex() const {
            throw std::runtime_error(std::format("resource {} does not have an rtv index", getName()));
        }
        virtual ShaderResourceAlloc::Index getSrvIndex() const {
            throw std::runtime_error(std::format("resource {} does not have an srv index", getName()));
        }
    };

    struct IRTVHandle {
        virtual ~IRTVHandle() = default;

        virtual RenderTargetAlloc::Index getRtvIndex() const = 0;
    };

    struct BasePassAttachment {
        virtual ~BasePassAttachment() = default;
        virtual IResourceHandle *getHandle() const = 0;

        BasePassAttachment(rhi::ResourceState requiredState)
            : requiredState(requiredState)
        { }

        rhi::ResourceState getRequiredState() const { return requiredState; }

    private:
        rhi::ResourceState requiredState;
    };

    template<typename T>
    struct PassAttachment : BasePassAttachment {
        PassAttachment(T *pHandle, rhi::ResourceState requiredState)
            : BasePassAttachment(requiredState)
            , pHandle(pHandle)
        { }

        T *getHandle() const override { return pHandle; }

    private:
        T *pHandle = nullptr;
    };

    struct IRenderPass : IGraphObject {
        virtual ~IRenderPass() = default;
        IRenderPass(Context *ctx, std::string name, StateDep stateDeps = eDepDevice)
            : IGraphObject(ctx, name, stateDeps)
        { }

        virtual void execute() = 0;

        std::vector<BasePassAttachment*> inputs;

    protected:
        template<typename T>
        PassAttachment<T> *addAttachment(T *pHandle, rhi::ResourceState requiredState) {
            auto *pResource = new PassAttachment<T>(pHandle, requiredState);
            inputs.push_back(pResource);
            return pResource;
        }
    };

    struct Graph {
        Graph(Context *ctx)
            : ctx(ctx)
        { }

        ~Graph() {
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

        Context *ctx;

    public:
        // TODO: make private
        std::vector<IRenderPass*> passes;
        std::vector<IResourceHandle*> resources;
    };
}
