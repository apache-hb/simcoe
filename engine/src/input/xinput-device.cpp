#include "engine/input/xinput-device.h"

#include "common.h"

using namespace simcoe;
using namespace simcoe::input;

namespace {
    constexpr UINT kLeftDeadzone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
    constexpr UINT kRightDeadzone = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
    constexpr float kTriggerDeadzone = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;

    bool setStickAxis(float& dstX, float& dstY, float stickX, float stickY, float deadzone) {
        bool bInDeadzone = sqrtf(stickX * stickX + stickY * stickY) < deadzone;

        bool bDirty = false;

        if (bInDeadzone) {
            if (dstX != 0.f) {
                dstX = 0.f;
                bDirty = true;
            }

            if (dstY != 0.f) {
                dstY = 0.f;
                bDirty = true;
            }

            return bDirty;
        }

        dstX = stickX / SHRT_MAX;
        dstY = stickY / SHRT_MAX;

        return true;
    }

    bool setTriggerRatio(float& dst, float stick, float deadzone) {
        if (stick > deadzone) {
            dst = stick / 255.f;
            return true;
        } else if (dst > 0.f) {
            dst = 0.f;
            return true;
        }

        return false;
    }

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
    : ISource(DeviceType::eXInput)
    , dwIndex(dwIndex)
{ }

// TODO: backoff on ERROR_DEVICE_NOT_CONNECTED
bool XInputGamepad::poll(State& state) {
    XINPUT_STATE xState;

    if (!bDeviceConnected) {
        if (!retryOnDisconnect.shouldRetry(clock.now())) {
            return false;
        }
    }

    DWORD dwErr = XInputGetState(dwIndex, &xState);

    if (dwErr == ERROR_DEVICE_NOT_CONNECTED) {
        bDeviceConnected = false;
        return false;
    }

    if (dwErr != ERROR_SUCCESS) {
        return false;
    }

    retryOnDisconnect.reset();

    XINPUT_GAMEPAD xPad = xState.Gamepad;

    bool bDirty = false;

    bDirty |= setStickAxis(state.axes[Axis::eGamepadLeftX], state.axes[Axis::eGamepadLeftY], xPad.sThumbLX, xPad.sThumbLY, kLeftDeadzone);
    bDirty |= setStickAxis(state.axes[Axis::eGamepadRightX], state.axes[Axis::eGamepadRightY], xPad.sThumbRX, xPad.sThumbRY, kRightDeadzone);

    bDirty |= setTriggerRatio(state.axes[Axis::eGamepadLeftTrigger], xPad.bLeftTrigger, kTriggerDeadzone);
    bDirty |= setTriggerRatio(state.axes[Axis::eGamepadRightTrigger], xPad.bRightTrigger, kTriggerDeadzone);

    for (const auto [slot, mask] : kGamepadButtons) {
        bDirty |= updateButton(state, slot, mask, xPad.wButtons);
    }

    return bDirty;
}

bool XInputGamepad::updateButton(State& result, Button button, WORD mask, WORD state) {
    bool bPressed = (state & mask) == mask;

    // if the button is not pressed, reset the key press index
    if (!bPressed) {
        return detail::update(result.buttons[button], size_t(0));
    }

    // if the button is already pressed theres no need to press it again
    if (result.buttons[button] != 0) {
        return false;
    }

    return detail::update(result.buttons[button], keyPressIndex++);
}
