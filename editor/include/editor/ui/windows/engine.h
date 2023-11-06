#pragma once

#include "editor/ui/service.h"
#include "editor/ui/components/buffer.h"

#include "game/tick.h"

namespace game { struct World; }

namespace editor::ui {
    struct EngineDebug final : ServiceDebug {
        EngineDebug(game::World *pWorld);

        void draw() override;

    private:
        game::World *pWorld = nullptr;
    };
}
