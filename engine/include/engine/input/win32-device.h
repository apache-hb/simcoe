#pragma once

#include "engine/os/system.h"
#include "engine/math/math.h"
#include "engine/input/input.h"

namespace simcoe::input {
    struct Win32Keyboard final : ISource {
        Win32Keyboard();

        bool poll(State& state) override;

        void handleMsg(UINT uMsg, WPARAM wParam, LPARAM lParam);
    private:
        void setKey(WORD wKey, size_t value);
        void setXButton(WORD wKey, size_t value);

        ButtonState buttons = {};
        size_t index = 1;
    };

    struct Win32Mouse final : ISource {
        Win32Mouse(bool bEnabled);

        bool poll(State& state) override;

        void captureInput(bool bShouldCapture);
        void update(Window *pWindow);

    private:
        math::int2 mouseOrigin = {};
        math::int2 mouseAbsolute = {};

        bool bMouseCaptured = false; ///< is the mouse captured (locked to the center of the screen)
        bool bMouseEnabled = false; ///< is the mouse enabled (should we read from it)
    };
}
