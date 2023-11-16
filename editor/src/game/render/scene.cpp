#include "game/render/scene.h"

#include "engine/depot/service.h"

using namespace game::render;

constexpr rhi::Display createDisplay(UINT width, UINT height) {
    rhi::Viewport viewport = {
        .width = float(width),
        .height = float(height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    rhi::Scissor scissor = {
        .right = LONG(width),
        .bottom = LONG(height)
    };

    return { viewport, scissor };
}

ScenePass::ScenePass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<IDSVHandle> *pDepthTarget)
    : IRenderPass(pGraph, "game.scene", eDepRenderSize)
{
    setRenderTargetHandle(pRenderTarget);
    setDepthStencilHandle(pDepthTarget);
}

void ScenePass::create() {
    const auto& createInfo = ctx->getCreateInfo();

    display = createDisplay(createInfo.renderWidth, createInfo.renderHeight);

    const rhi::GraphicsPipelineInfo psoCreateInfo = {
        .vertexShader = DepotService::openFile("object.vs.cso")->blob(),
        .pixelShader = DepotService::openFile("object.ps.cso")->blob(),

        .attributes = {
            { "POSITION", offsetof(Vertex, position), rhi::TypeFormat::eFloat3 },
            { "TEXCOORD", offsetof(Vertex, uv), rhi::TypeFormat::eFloat2 }
        },

        .textureInputs = {
            { "tex", rhi::InputVisibility::ePixel, 0, true }
        },

        .uniformInputs = {
            { "camera", rhi::InputVisibility::eVertex, 0, false },
            { "object", rhi::InputVisibility::eVertex, 1, false }
        },

        .samplers = {
            { rhi::InputVisibility::ePixel, 0 }
        },

        .rtvFormat = ctx->getSwapChainFormat(),
        .depthEnable = true,
        .dsvFormat = ctx->getDepthFormat()
    };

    pPipeline = ctx->createGraphicsPipeline(psoCreateInfo);
    pPipeline->setName("pso.game");

    std::lock_guard guard(lock);
    
    batch = {};
    newBatch = {};
    bDirtyBatch = false;
}

void ScenePass::destroy() {
    delete pPipeline;
    
    std::lock_guard guard(lock);

    batch = {};
    newBatch = {};
    bDirtyBatch = false;
}

void ScenePass::execute() {
    ctx->setDisplay(display);

    if (bDirtyBatch && lock.try_lock()) {
        batch = std::move(newBatch);
        bDirtyBatch = false;
        lock.unlock();
    }

    for (auto& action : batch.actions) {
        action(this, ctx);
    }
}

void ScenePass::update(CommandBatch&& updateBatch) {
    std::lock_guard guard(lock);
    newBatch = std::move(updateBatch);
    bDirtyBatch = true;
}
