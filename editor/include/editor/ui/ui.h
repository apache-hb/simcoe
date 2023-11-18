#pragma once

#include "engine/render/graph.h"

#include "imgui/imgui.h"

#include <functional>

namespace editor::ui {
    // debug handles
    struct DebugHandle {
        virtual ~DebugHandle() = default;
        DebugHandle(const std::string &name, std::function<void()> fn)
            : name(name)
            , fn(fn)
        { }

        void setEnabled(bool bUpdate) { bEnabled = bUpdate; }
        bool isEnabled() const { return bEnabled; }

        const char *getName() const { return name.c_str(); }
        void draw() const { fn(); }

    private:
        bool bEnabled = true;

        std::string name;
        std::function<void()> fn;
    };

    using GlobalHandle = std::unique_ptr<DebugHandle, void(*)(DebugHandle*)>;
    using LocalHandle = std::unique_ptr<DebugHandle>;

    void removeGlobalHandle(DebugHandle *pHandle);
    GlobalHandle addGlobalHandle(const std::string &name, std::function<void()> draw);
    void enumGlobalHandles(std::function<void(DebugHandle*)> callback);
}
