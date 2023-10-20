#pragma once

#include "engine/render/graph.h"

#include <functional>

namespace editor::debug {
    using namespace simcoe;

    struct DebugHandle {
        std::string name;
        std::function<void()> draw;
    };

    void removeHandle(DebugHandle *pHandle);

    using UserHandle = std::unique_ptr<DebugHandle, decltype(&removeHandle)>;

    UserHandle addHandle(const std::string &name, std::function<void()> draw);

    void enumHandles(std::function<void(DebugHandle*)> callback);

    void showDebugGui(render::Graph *pGraph);
}
