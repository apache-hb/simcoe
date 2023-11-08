#include "editor/ui/windows/engine.h"

#include "game/world.h"

#include "implot/implot.h"

using namespace editor;
using namespace editor::ui;

EngineDebug::EngineDebug(game::World *pWorld)
    : ServiceDebug("Engine")
    , pWorld(pWorld)
{
    SM_ASSERT(pWorld != nullptr);
}

void EngineDebug::draw() {

}
