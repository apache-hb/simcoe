#pragma once

#include "editor/ui/service.h"

#include "engine/threads/service.h"

namespace editor::ui {
    namespace ts = simcoe::threads;
    struct ThreadServiceUi final : ServiceUi {
        ThreadServiceUi();

        void draw() override;

    private:
        void drawPackage(const ts::Package& package) const;
        ts::CoreIndex getFastestCore(const ts::Chiplet& chiplet) const;

        int workers = 0;

        ts::Geometry geometry = {};
        std::unordered_map<ts::ChipletIndex, ts::CoreIndex> fastestCores = {};
    };
}
