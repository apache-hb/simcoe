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
    for (const PassResource *pInput : pPass->inputs) {
        auto [pHandle, requiredState] = *pInput;
        auto [pResource, actualState] = *pHandle;

        if (actualState != requiredState) {
            ctx->transition(pResource, actualState, pInput->requiredState);
            pHandle->state = requiredState;
        }
    }

    pPass->execute(ctx);
}
