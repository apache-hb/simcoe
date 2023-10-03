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

        if (pHandle->currentState != requiredState) {
            ctx->transition(pHandle->getResource(), pHandle->currentState, pInput->requiredState);
            pHandle->currentState = requiredState;
        }
    }

    pPass->execute(ctx);
}
