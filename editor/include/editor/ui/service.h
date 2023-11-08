#pragma once

#include "editor/ui/ui.h"

namespace editor::ui {
    // service ui panel
    struct ServiceUi {
        virtual ~ServiceUi() = default;

        std::string_view getServiceName() const;
        std::string_view getServiceError() const;

        void drawMenuItem();
        virtual void drawWindow();

        virtual void draw() = 0;
    protected:
        ServiceUi(std::string_view name)
            : serviceName(name)
        { }

        void setServiceError(std::string_view reason);

        bool bOpen = true;

    private:
        std::string_view serviceName = "";
        std::string_view serviceError = "";
    };
}
