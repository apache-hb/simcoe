#include "editor/render/graph.h"

using namespace editor;

void RenderGraph::resizeDisplay(UINT width, UINT height) {
    const auto& createInfo = ctx->getCreateInfo();
    if (width == createInfo.displayWidth && height == createInfo.displayHeight)
        return;

    lock = true;
    std::lock_guard guard(renderLock);

    ctx->waitForDirectQueue();
    ctx->waitForCopyQueue();

    destroyIf(StateDep::eDepDisplaySize);

    ctx->changeDisplaySize(width, height);

    createIf(StateDep::eDepDisplaySize);

    lock = false;
}

void RenderGraph::resizeRender(UINT width, UINT height) {
    const auto& createInfo = ctx->getCreateInfo();
    if (width == createInfo.renderWidth && height == createInfo.renderHeight)
        return;

    lock = true;
    std::lock_guard guard(renderLock);

    ctx->waitForDirectQueue();
    ctx->waitForCopyQueue();

    destroyIf(StateDep::eDepRenderSize);

    ctx->changeRenderSize(width, height);

    createIf(StateDep::eDepRenderSize);

    lock = false;
}

void RenderGraph::changeBackBufferCount(UINT count) {
    const auto& createInfo = ctx->getCreateInfo();
    if (count == createInfo.backBufferCount)
        return;

    lock = true;
    std::lock_guard guard(renderLock);

    ctx->waitForDirectQueue();
    ctx->waitForCopyQueue();

    destroyIf(StateDep::eDepBackBufferCount);

    ctx->changeBackBufferCount(count);

    createIf(StateDep::eDepBackBufferCount);

    lock = false;
}

void RenderGraph::changeAdapter(UINT index) {
    const auto& createInfo = ctx->getCreateInfo();
    if (index == createInfo.adapterIndex)
        return;

    lock = true;
    std::lock_guard guard(renderLock);

    ctx->waitForDirectQueue();
    ctx->waitForCopyQueue();

    destroyIf(StateDep::eDepDevice);

    ctx->changeAdapter(index);

    createIf(StateDep::eDepDevice);

    lock = false;
}

void RenderGraph::execute() {
    if (lock) { return; }

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
