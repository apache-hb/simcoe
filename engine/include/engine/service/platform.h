#pragma once

#include "engine/service/service.h"

#include <array>
#include <windows.h>

namespace simcoe {
    struct PlatformService final : IStaticService<PlatformService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "platform";
        static constexpr std::array<std::string_view, 0> kServiceDeps = {};

        // IService
        void createService() override;
        void destroyService() override;

        // PlatformService
        static void setup(HINSTANCE hInstance, int nCmdShow);

    private:
        HINSTANCE hInstance = nullptr;
        int nCmdShow = -1;

        LONGLONG frequency = 0;
    };
}
