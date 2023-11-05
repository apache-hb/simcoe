#pragma once

#include "editor/ui/service.h"

#include "engine/threads/service.h"

namespace editor::ui {
    namespace ts = simcoe::threads;
    struct ThreadServiceDebug final : ServiceDebug {
        ThreadServiceDebug();

        void draw() override;

    private:
        void drawPackage(const ts::Package& package) const;
        ts::CoreIndex getFastestCore(const ts::Chiplet& chiplet) const;

        ts::Geometry geometry = {};
        std::unordered_map<ts::ChipletIndex, ts::CoreIndex> fastestCores = {};

        std::string_view queryThreadName(ts::ThreadId id);
        std::unordered_map<ts::ThreadId, std::string> threadNames = {};
    };
}
