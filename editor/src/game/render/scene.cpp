#include "game/render/scene.h"

using namespace game::graph;

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
}

void ScenePass::destroy() {

}

void ScenePass::execute() {
    ctx->setDisplay(display);

    if (bDirtyBatch && lock.try_lock()) {
        batch = std::move(newBatch);
        bDirtyBatch = false;
        lock.unlock();
    }

    for (auto& action : batch.actions) {
        action(pGraph, ctx);
    }
}

void ScenePass::update(CommandBatch&& updateBatch) {
    LOG_INFO("update");

    std::lock_guard guard(lock);
    newBatch = std::move(updateBatch);
    bDirtyBatch = true;
}
