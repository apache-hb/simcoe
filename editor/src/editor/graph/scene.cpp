#include "editor/graph/scene.h"

#include <array>

using namespace editor;
using namespace editor::graph;

///
/// create display helper functions
///

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

ScenePass::ScenePass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget)
    : IRenderPass(pGraph, "scene", eDepRenderSize)
{
    setRenderTargetHandle(pRenderTarget);
}

void ScenePass::create() {
    const auto& createInfo = ctx->getCreateInfo();
    display = createDisplay(createInfo.renderWidth, createInfo.renderHeight);
}

void ScenePass::destroy() {

}

void ScenePass::execute() {
    ctx->setDisplay(display);
}
