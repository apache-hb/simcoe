#include "editor/ui/panels/world.h"

#include "game/world.h"

#include "implot/implot.h"

using namespace editor;
using namespace editor::ui;

WorldUi::WorldUi(game::World *pWorld)
    : ServiceUi("World")
    , pWorld(pWorld)
{
    SM_ASSERT(pWorld != nullptr);
}

void WorldUi::draw() {

}
