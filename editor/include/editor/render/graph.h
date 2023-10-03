#pragma once

#include "editor/render/render.h"

namespace editor {
    struct ResourceHandle {
        render::DeviceResource *pResource;
        render::ResourceState state;
    };

    struct PassResource {
        ResourceHandle *pHandle;
        render::ResourceState requiredState;
    };

    struct IRenderPass {
        virtual ~IRenderPass() = default;

        virtual void create(RenderContext *ctx) = 0;
        virtual void destroy(RenderContext *ctx) = 0;

        virtual void execute(RenderContext *ctx) = 0;
        
        PassResource *addInput(render::ResourceState requiredState);

        std::vector<PassResource*> inputs;
    };

    struct RenderGraph {
        RenderGraph(RenderContext *ctx)
            : ctx(ctx) 
        { }

        ~RenderGraph() {
            for (IRenderPass *pPass : passes) {
                pPass->destroy(ctx);
                delete pPass;
            }
        }

        void addPass(IRenderPass *pPass) { 
            pPass->create(ctx);
            passes.push_back(pPass); 
        }

        ResourceHandle *addResource(render::DeviceResource *pResource, render::ResourceState state) {
            ResourceHandle *pHandle = new ResourceHandle{ pResource, state };
            resources.push_back(pHandle);
            return pHandle;
        }

        void execute();
    private:
        void executePass(IRenderPass *pPass);

        RenderContext *ctx;
        std::vector<IRenderPass*> passes;
        std::vector<ResourceHandle*> resources;
    };
}
