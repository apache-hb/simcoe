#pragma once

#include "engine/core/mt.h"

#include "engine/input/win32-device.h"
#include "engine/input/xinput-device.h"

#include "engine/service/platform.h"
#include "engine/threads/service.h"

namespace simcoe {
    struct InputService final : IStaticService<InputService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "input";
        static constexpr std::array kServiceDeps = {
            PlatformService::kServiceName,
            ThreadService::kServiceName
        };

        // IService
        bool createService() override;
        void destroyService() override;

        // InputService
        static void addSource(input::ISource *pSource);
        static void addClient(input::IClient *pClient);

        static void pollInput();
        static void handleMsg(UINT uMsg, WPARAM wParam, LPARAM lParam);

        static mt::SharedMutex &getMutex();
        static input::Manager &getManager();
    };
}
