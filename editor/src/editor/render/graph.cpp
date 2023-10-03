#include "editor/render/graph.h"

using namespace editor;

void RenderGraph::execute() {
    ctx->beginRender();
    for (IRenderPass *pPass : passes) {
        executePass(pPass);
    }
    ctx->endRender();
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
