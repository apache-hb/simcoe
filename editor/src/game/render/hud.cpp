#include "game/render/hud.h"

using namespace game::render;

HudPass::HudPass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget)
    : IRenderPass(pGraph, "game.hud")
{
    setRenderTargetHandle(pRenderTarget);
}

void HudPass::create() {

}

void HudPass::destroy() {

}

void HudPass::execute() {

}
