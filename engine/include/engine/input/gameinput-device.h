#pragma once

#include "vendor/microsoft/gdk.h"

#include "engine/input/input.h"
#include "engine/engine.h"

namespace simcoe::input {
    struct GameInput final : ISource {
        GameInput();
        ~GameInput();

        bool poll(State& state) override;

    private:
        bool updateButton(State& state, Button button, GameInputGamepadButtons input, GameInputGamepadButtons mask);

        IGameInput *pInput = nullptr;
        IGameInputDevice *pDevice = nullptr;

        ReportOnce getStateError;
        ReportOnce deviceGeometry;
        size_t buttonPressIndex;
    };
}
