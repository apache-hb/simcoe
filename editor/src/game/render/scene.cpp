#include "game/render/scene.h"

using namespace game::graph;

ScenePass::ScenePass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<IDSVHandle> *pDepthTarget)
    : IRenderPass(pGraph, "game.scene")
{
    setRenderTargetHandle(pRenderTarget);
    setDepthStencilHandle(pDepthTarget);
}

void ScenePass::create() {

}

void ScenePass::destroy() {

}

void ScenePass::execute() {

}
