#pragma once

#include "editor/debug/debug.h"

#include "imgui/imgui.h"

namespace editor::debug {
    struct DepotDebug final : ServiceDebug {
        DepotDebug();

        void draw() override;
    };
}
