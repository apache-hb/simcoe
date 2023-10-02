#pragma once

#include "editor/render/render.h"

#include <unordered_map>

namespace editor {
    struct RenderGraph;
    struct IGraphPass;
    struct IGraphAsset;

    struct IGraphInput {
        virtual ~IGraphInput() = default;

        render::ResourceState expectedState;
    };

    struct IGraphOutput {
        virtual ~IGraphOutput() = default;

        render::ResourceState providedState;
    };

    struct IRenderGraph {
        virtual ~IRenderGraph();

        template<typename T, typename... A>
        T *addPass(A&&... args) {
            auto pPass = new T(std::forward<A>(args)...);
            addPass(pPass);
            return pPass;
        }

        void execute(IGraphPass *pPass);

    protected:
        // construction
        IRenderGraph(RenderContext *ctx) : ctx(ctx) { }
        RenderContext *ctx;

        // pass management
        void addPass(IGraphPass *pPass);
        std::vector<IGraphPass*> passes;

        // link management
        std::unordered_map<IGraphInput*, IGraphOutput*> links;
    };

    struct IGraphPass {
        virtual ~IGraphPass() = default;

        virtual void create(RenderContext *ctx) = 0;
        virtual void destroy(RenderContext *ctx) = 0;

        virtual void execute(RenderContext *ctx) = 0;

    protected:
        IGraphInput *addInput(render::ResourceState expectedState);
        IGraphOutput *addOutput(render::ResourceState providedState);
    
        std::vector<IGraphInput*> inputs;
        std::vector<IGraphOutput*> outputs;
    };

    struct IGraphAsset {
        virtual ~IGraphAsset() = default;

        virtual void load() = 0;
        virtual void unload() = 0;

        render::DeviceResource *getResource() const { return pResource; }
        std::string_view getName() const { return name; }

    protected:
        void setResource(render::DeviceResource *pResource) {
            this->pResource = pResource;
        }

        IGraphAsset(const std::string& name) : name(name) { }

    private:
        std::string name;
        render::DeviceResource *pResource;
    };
}
