#pragma once

#include "vendor/microsoft/gdk.h"

#include "engine/input/input.h"

namespace simcoe::input {
    struct GameInput final : ISource {
        GameInput();
        ~GameInput();

        bool poll(State& state) override;

    private:
        IGameInput *pInput;

        IGameInputDevice *pDevice;
    };
}
