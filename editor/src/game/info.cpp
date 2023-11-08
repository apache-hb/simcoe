#include "game/info.h"

using namespace game;

void WorldInfo::verify() const {
    SM_ASSERT(entityLimit > 0);

    SM_ASSERT(pRenderContext != nullptr);
    SM_ASSERT(pRenderGraph != nullptr);
    SM_ASSERT(renderFaultLimit > 0);

    SM_ASSERT(pHudPass != nullptr);
    SM_ASSERT(pScenePass != nullptr);
}
