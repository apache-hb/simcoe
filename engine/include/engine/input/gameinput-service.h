#pragma once

#include "vendor/microsoft/gdk.h"
#include "engine/input/input.h"

namespace simcoe::input {
    struct GameInputService {
        GameInputService(input::Manager *pManager);
        ~GameInputService();

    private:
        static void onDeviceEvent(
            GameInputCallbackToken token,
            void *pContext,
            IGameInputDevice *pDevice,
            uint64_t timestamp,
            GameInputDeviceStatus currentStatus,
            GameInputDeviceStatus previousStatus
        );

        struct Device : input::ISource {
            Device(input::Manager *pManager, IGameInputDevice *pDevice);
            ~Device();

            bool poll(input::State& state) override;

        private:
            input::Manager *pManager = nullptr;
            IGameInputDevice *pDevice = nullptr;
        };

        input::Manager *pManager = nullptr;

        IGameInput *pInstance = nullptr;
        GameInputCallbackToken eventToken = 0;


        void addDevice(IGameInputDevice *pDevice);
        void removeDevice(IGameInputDevice *pDevice);

        std::unordered_map<IGameInputDevice*, Device*> loadedDevices;
    };
}
