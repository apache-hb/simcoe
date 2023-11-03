#pragma once

#include "engine/core/mt.h"

#include "engine/input/win32-device.h"
#include "engine/input/xinput-device.h"

#include "engine/service/platform.h"
#include "engine/threads/service.h"

namespace simcoe {
    struct InputService final : IStaticService<InputService> {
        InputService();

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

        static void pollInput() {
            mt::read_lock lock(getMutex());
            getManager().poll();
        }

        static void handleMsg(UINT uMsg, WPARAM wParam, LPARAM lParam) {
            mt::read_lock lock(getMutex());

            if (get()->pKeyboard) {
                get()->pKeyboard->handleMsg(uMsg, wParam, lParam);
            }
        }

    private:
        mt::shared_mutex mutex;
        input::Manager manager;
        threads::ThreadHandle *pThread = nullptr;

        input::Win32Keyboard *pKeyboard = nullptr;
        input::Win32Mouse *pMouse = nullptr;
        input::XInputGamepad *pGamepad0 = nullptr;

        size_t pollInterval = 16; ///< how often to poll input devices (in ms)

        bool bEnableKeyboard = true;
        bool bEnableMouse = true;
        bool bLockMouse = false;

        bool bEnableGamepad0 = true;

    public:
        static mt::shared_mutex &getMutex() { return get()->mutex; }
        static input::Manager &getManager() { return get()->manager; }
    };
}
