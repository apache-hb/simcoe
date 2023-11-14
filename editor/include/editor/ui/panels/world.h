#pragma once

#include "editor/ui/service.h"
#include "editor/ui/components/buffer.h"

namespace game { struct World; }

namespace editor::ui {
    struct WorldUi final : ServiceUi {
        WorldUi(game::World *pWorld);

        void draw() override;

    private:
        game::World *pWorld = nullptr;
    };
}
