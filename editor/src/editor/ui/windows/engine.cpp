#include "editor/ui/windows/engine.h"

#include "game/world.h"

#include "implot/implot.h"

using namespace editor;
using namespace editor::ui;

EngineDebug::EngineDebug(game::World *pWorld)
    : ServiceDebug("Engine")
    , pWorld(pWorld)
{
    ASSERTF(pWorld != nullptr, "World is null");
}

void EngineDebug::draw() {

}
