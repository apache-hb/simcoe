#pragma once

#include "editor/ui/service.h"

namespace editor::ui {
    struct GdkDebug final : ServiceDebug {
        GdkDebug();

        void draw() override;
    };
}
