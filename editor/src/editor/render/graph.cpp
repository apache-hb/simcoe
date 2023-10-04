#include "editor/render/graph.h"

using namespace editor;

void RenderGraph::resizeDisplay(UINT width, UINT height) {
    const auto& createInfo = ctx->getCreateInfo();
    if (width == createInfo.displayWidth && height == createInfo.displayHeight)
        return;

    std::lock_guard guard(renderLock);

    ctx->waitForDirectQueue();
    ctx->waitForCopyQueue();

    destroyIf(StateDep::eDepDisplaySize);

    ctx->changeDisplaySize(width, height);

    createIf(StateDep::eDepDisplaySize);
}

void RenderGraph::execute() {
    std::lock_guard guard(renderLock);

    ctx->beginRender();
    ctx->beginDirect();

    for (IRenderPass *pPass : passes) {
        executePass(pPass);
    }

    ctx->endDirect();
    ctx->endRender();
    ctx->waitForDirectQueue();
}

void RenderGraph::createIf(StateDep dep) {
    for (IResourceHandle *pHandle : resources) {
        if (pHandle->stateDeps & dep) pHandle->create(ctx);
    }

    for (IRenderPass *pPass : passes) {
        if (pPass->stateDeps & dep) pPass->create(ctx);
    }
}

void RenderGraph::destroyIf(StateDep dep) {
    for (IRenderPass *pPass : passes) {
        if (pPass->stateDeps & dep) pPass->destroy(ctx);
    }

    for (IResourceHandle *pHandle : resources) {
        if (pHandle->stateDeps & dep) pHandle->destroy(ctx);
    }
}

void RenderGraph::executePass(IRenderPass *pPass) {
    for (const auto *pInput : pPass->inputs) {
        auto *pHandle = pInput->getHandle();
        auto requiredState = pInput->requiredState;
        auto currentState = pHandle->getCurrentState(ctx);

        if (currentState != requiredState) {
            ctx->transition(pHandle->getResource(ctx), currentState, pInput->requiredState);
            pHandle->setCurrentState(ctx, requiredState);
        }
    }

    pPass->execute(ctx);
}
