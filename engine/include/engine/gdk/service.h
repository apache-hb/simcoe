#pragma once

#include "vendor/microsoft/gdk.h"
#include "engine/input/input.h"

namespace simcoe::gdk {
    struct Service {
        Service(input::Manager *pManager);
        ~Service();

        void poll();

    private:
        struct Device : input::ISource {
            Device(input::Manager *pManager, IGameInputDevice *pDevice);
            ~Device();

            bool poll(input::State& state) override;

        private:
            IGameInputDevice *pDevice = nullptr;
        };

        input::Manager *pManager = nullptr;

        IGameInput *pInstance = nullptr;
        std::unordered_map<IGameInputDevice*, Device*> loadedDevices;
    };
}
