#include "engine/input/xinput-device.h"

#include "common.h"

using namespace simcoe;
using namespace simcoe::input;

namespace {
    struct GamepadKeyMapping {
        Button slot;
        WORD mask;
    };

    constexpr auto kGamepadButtons = std::to_array<GamepadKeyMapping>({
        { Button::ePadBumperLeft, XINPUT_GAMEPAD_LEFT_SHOULDER },
        { Button::ePadBumperRight, XINPUT_GAMEPAD_RIGHT_SHOULDER },

        { Button::ePadButtonUp, XINPUT_GAMEPAD_Y },
        { Button::ePadButtonDown, XINPUT_GAMEPAD_A },
        { Button::ePadButtonLeft, XINPUT_GAMEPAD_X },
        { Button::ePadButtonRight, XINPUT_GAMEPAD_B },

        { Button::ePadDirectionUp, XINPUT_GAMEPAD_DPAD_UP },
        { Button::ePadDirectionDown, XINPUT_GAMEPAD_DPAD_DOWN },
        { Button::ePadDirectionLeft, XINPUT_GAMEPAD_DPAD_LEFT },
        { Button::ePadDirectionRight, XINPUT_GAMEPAD_DPAD_RIGHT },

        { Button::ePadStart, XINPUT_GAMEPAD_START },
        { Button::ePadBack, XINPUT_GAMEPAD_BACK },

        { Button::ePadStickLeft, XINPUT_GAMEPAD_LEFT_THUMB },
        { Button::ePadStickRight, XINPUT_GAMEPAD_RIGHT_THUMB },
    });
}

XInputGamepad::XInputGamepad(DWORD dwIndex)
    : ISource(DeviceType::eDeviceXInput)
    , dwIndex(dwIndex)
{ }

bool XInputGamepad::poll(State& state) {
    XINPUT_STATE xState;

    if (DWORD dwErr = XInputGetState(dwIndex, &xState); dwErr != ERROR_SUCCESS) {
        return false;
    }

    bool dirty = false;

    for (const auto [slot, mask] : kGamepadButtons) {
        dirty |= updateButton(state, slot, mask, xState.Gamepad.wButtons);
    }

    return dirty;
}

bool XInputGamepad::updateButton(State& result, Button button, WORD mask, WORD state) {
    bool bPressed = (state & mask) == mask;

    // if the button is not pressed, reset the key press index
    if (!bPressed) {
        return detail::update(result.buttons[button], size_t(0));
    }

    if (result.buttons[button] != 0) { return false; }

    return detail::update(result.buttons[button], keyPressIndex++);
}
