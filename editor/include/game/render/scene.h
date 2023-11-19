#pragma once

#include "engine/render/assets.h"
#include "editor/graph/assets.h"

#include "engine/render/graph.h"

#include "engine/threads/mutex.h"

#include <unordered_set>

namespace game::render {
    using namespace editor::graph;
    using namespace simcoe::math;
    using namespace simcoe::render;

    struct ScenePass;

    using SceneAction = std::function<void(ScenePass *, Context *)>;

    struct UNIFORM_BUFFER Model {
        float4x4 model;
    };

    struct UNIFORM_BUFFER Camera {
        float4x4 view;
        float4x4 proj;
    };

    struct ModelUniform final : public IUniformHandle<Model> {
        using Super = IUniformHandle<Model>;
        using Super::Super;
    };

    struct CameraUniform final : public IUniformHandle<Camera> {
        using Super = IUniformHandle<Camera>;
        using Super::Super;
    };

    struct CommandBatch {
        void add(SceneAction&& action) {
            actions.emplace_back(std::move(action));
        }

        std::vector<SceneAction> actions;
    };

    struct ScenePass final : IRenderPass {
        ScenePass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<IDSVHandle> *pDepthTarget);

        // IRenderPass
        void create() override;
        void destroy() override;

        void execute() override;

        void update(CommandBatch&& updateBatch);

        Graph *getGraph() const { return pGraph; }
        Context *getContext() const { return ctx; }

        RenderTargetAlloc::Index getRenderTargetIndex() const { 
            return getRenderTarget()->getRtvIndex();
        }

        rhi::PipelineState *getPipeline() const {
            return pPipeline;
        }

        UINT textureReg() const { return pPipeline->getTextureInput("tex");}
        UINT modelReg() const { return pPipeline->getUniformInput("object"); }
        UINT cameraReg() const { return pPipeline->getUniformInput("camera"); }

    private:
        rhi::Display display;
        rhi::PipelineState *pPipeline = nullptr;

        CommandBatch batch;

        CommandBatch newBatch;
        bool bDirtyBatch = false;
        mt::Mutex lock{"batch"};
    };
}
