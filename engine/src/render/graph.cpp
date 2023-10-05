#include "engine/render/graph.h"

using namespace simcoe;
using namespace simcoe::render;

void Graph::resizeDisplay(UINT width, UINT height) {
    const auto& createInfo = ctx->getCreateInfo();
    if (width == createInfo.displayWidth && height == createInfo.displayHeight)
        return;

    changeData(StateDep::eDepDisplaySize, [=] {
        ctx->changeDisplaySize(width, height);
    });
}

void Graph::resizeRender(UINT width, UINT height) {
    const auto& createInfo = ctx->getCreateInfo();
    if (width == createInfo.renderWidth && height == createInfo.renderHeight)
        return;

    changeData(StateDep::eDepRenderSize, [=] {
        ctx->changeRenderSize(width, height);
    });
}

void Graph::changeBackBufferCount(UINT count) {
    const auto& createInfo = ctx->getCreateInfo();
    if (count == createInfo.backBufferCount)
        return;

    changeData(StateDep::eDepBackBufferCount, [=] {
        ctx->changeBackBufferCount(count);
    });
}

void Graph::changeAdapter(UINT index) {
    const auto& createInfo = ctx->getCreateInfo();
    if (index == createInfo.adapterIndex)
        return;

    changeData(StateDep::eDepDevice, [=] {
        ctx->changeAdapter(index);
    });
}

void Graph::execute() {
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

void Graph::createIf(StateDep dep) {
    for (IResourceHandle *pHandle : resources) {
        if (pHandle->dependsOn(dep)) pHandle->create();
    }

    for (IRenderPass *pPass : passes) {
        if (pPass->dependsOn(dep)) pPass->create();
    }
}

void Graph::destroyIf(StateDep dep) {
    for (IRenderPass *pPass : passes) {
        if (pPass->dependsOn(dep)) pPass->destroy();
    }

    for (IResourceHandle *pHandle : resources) {
        if (pHandle->dependsOn(dep)) pHandle->destroy();
    }
}

void Graph::executePass(IRenderPass *pPass) {
    for (const auto *pInput : pPass->inputs) {
        auto *pHandle = pInput->getHandle();
        auto requiredState = pInput->requiredState;
        auto currentState = pHandle->getCurrentState();

        if (currentState != requiredState) {
            ctx->transition(pHandle->getResource(), currentState, pInput->requiredState);
            pHandle->setCurrentState(requiredState);
        }
    }

    pPass->execute();
}