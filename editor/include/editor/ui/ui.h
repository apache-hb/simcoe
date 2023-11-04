#pragma once

#include "engine/render/graph.h"

#include "imgui/imgui.h"

#include <functional>

namespace editor::ui {
    using namespace simcoe;

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

    // service debuggers
    struct ServiceDebug {
        virtual ~ServiceDebug() = default;

        std::string_view getServiceName() const;
        std::string_view getServiceError() const;

        void drawMenuItem();
        virtual void drawWindow();

        virtual void draw() = 0;
    protected:
        ServiceDebug(std::string_view name)
            : serviceName(name)
        { }

        void setServiceError(std::string_view reason);

        bool bOpen = true;

    private:
        std::string_view serviceName = "";
        std::string_view serviceError = "";
    };
}
