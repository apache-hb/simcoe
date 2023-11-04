#pragma once

#include "editor/ui/ui.h"

namespace editor::ui {
    struct GdkDebug final : ServiceDebug {
        GdkDebug();

        void draw() override;
    };
}
