#pragma once

#include "editor/ui/service.h"

namespace editor::ui {
    struct GameRuntimeUi final : ServiceUi {
        GameRuntimeUi();

        void draw() override;
    };
}
