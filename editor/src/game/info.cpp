#include "game/info.h"

using namespace game;

void WorldInfo::verify() const {
    ASSERTF(entityLimit > 0, "entityLimit must be greater than 0");

    ASSERTF(pInput != nullptr, "pInput must not be null");

    ASSERTF(pRenderContext != nullptr, "pRender must not be null");
    ASSERTF(pRenderGraph != nullptr, "pRenderGraph must not be null");
    ASSERTF(renderFaultLimit > 0, "renderFaultLimit must be greater than 0");

    ASSERTF(pHudPass != nullptr, "pHudPass must not be null");
    ASSERTF(pScenePass != nullptr, "pScenePass must not be null");
}
