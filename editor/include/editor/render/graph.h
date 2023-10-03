#pragma once

#include "editor/render/render.h"

namespace editor {
    struct IResourceHandle {
        virtual ~IResourceHandle() = default;

        virtual void create(RenderContext *ctx) = 0;
        virtual void destroy(RenderContext *ctx) = 0;

        virtual render::DeviceResource* getResource() const = 0;

        render::ResourceState currentState;
        RenderTargetAlloc::Index rtvIndex;
        DataAlloc::Index srvIndex;
    };

    struct PassResource {
        IResourceHandle *pHandle;
        render::ResourceState requiredState;
    };

    struct IRenderPass {
        virtual ~IRenderPass() = default;

        virtual void create(RenderContext *ctx) = 0;
        virtual void destroy(RenderContext *ctx) = 0;

        virtual void execute(RenderContext *ctx) = 0;
        
        PassResource *addResource(IResourceHandle *pHandle, render::ResourceState requiredState) {
            PassResource *pResource = new PassResource{ pHandle, requiredState };
            inputs.push_back(pResource);
            return pResource;
        }

        std::vector<PassResource*> inputs;
    };

    struct RenderGraph {
        RenderGraph(RenderContext *ctx)
            : ctx(ctx) 
        { }

        ~RenderGraph() {
            ctx->endRender();
            
            for (IRenderPass *pPass : passes) {
                pPass->destroy(ctx);
                delete pPass;
            }

            for (IResourceHandle *pHandle : resources) {
                pHandle->destroy(ctx);
                delete pHandle;
            }
        }

        void addPass(IRenderPass *pPass) { 
            pPass->create(ctx);
            passes.push_back(pPass); 
        }

        void addResource(IResourceHandle *pHandle) {
            pHandle->create(ctx);
            resources.push_back(pHandle);
        }

        void execute();
    private:
        void executePass(IRenderPass *pPass);

        RenderContext *ctx;
        std::vector<IRenderPass*> passes;
        std::vector<IResourceHandle*> resources;
    };
}
