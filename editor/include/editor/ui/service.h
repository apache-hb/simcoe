#pragma once

#include "editor/ui/ui.h"

namespace editor::ui {
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
