#pragma once

#include "editor/ui/service.h"
#include "editor/ui/components/buffer.h"

namespace editor::ui {
    struct WorldUi final : ServiceUi {
        WorldUi();

        void draw() override;
    };
}
