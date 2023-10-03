#include "editor/render/graph.h"

using namespace editor;

void RenderGraph::resize(UINT width, UINT height) {
    std::lock_guard guard(renderLock);

    ctx->flush();

    destroy();

    ctx->changeDisplaySize(width, height);

    create();
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

void RenderGraph::create() {
    for (IResourceHandle *pHandle : resources) {
        pHandle->create(ctx);
    }

    for (IRenderPass *pPass : passes) {
        pPass->create(ctx);
    }
}

void RenderGraph::destroy() {
    for (IRenderPass *pPass : passes) {
        pPass->destroy(ctx);
    }

    for (IResourceHandle *pHandle : resources) {
        pHandle->destroy(ctx);
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
