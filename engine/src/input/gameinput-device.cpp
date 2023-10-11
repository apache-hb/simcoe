#include "engine/input/gameinput-device.h"
#include "common.h"

#include "engine/os/system.h"

#include <wrl.h>

using Microsoft::WRL::ComPtr;

using namespace simcoe;
using namespace simcoe::input;

#define HR_CHECK(expr) \
    do { \
        if (HRESULT hr = (expr); FAILED(hr)) { \
            simcoe::logError("failure: {} ({})", #expr, simcoe::getErrorName(hr)); \
            throw std::runtime_error(#expr); \
        } \
    } while (false)

namespace {
    struct GameInputKeyMapping {
        Button slot;
        GameInputGamepadButtons mask;
    };

    constexpr auto kGamepadButtons = std::to_array<GameInputKeyMapping>({
        { Button::ePadBumperLeft, GameInputGamepadLeftShoulder },
        { Button::ePadBumperRight, GameInputGamepadRightShoulder },

        { Button::ePadButtonUp, GameInputGamepadY },
        { Button::ePadButtonDown, GameInputGamepadA },
        { Button::ePadButtonLeft, GameInputGamepadX },
        { Button::ePadButtonRight, GameInputGamepadB },

        { Button::ePadDirectionUp, GameInputGamepadDPadUp },
        { Button::ePadDirectionDown, GameInputGamepadDPadDown },
        { Button::ePadDirectionLeft, GameInputGamepadDPadLeft },
        { Button::ePadDirectionRight, GameInputGamepadDPadRight },

        { Button::ePadStart, GameInputGamepadMenu },
        { Button::ePadBack, GameInputGamepadView },

        { Button::ePadStickLeft, GameInputGamepadLeftThumbstick },
        { Button::ePadStickRight, GameInputGamepadRightThumbstick },
    });

    bool setStickAxis(float& dstX, float& dstY, float stickX, float stickY) {
        dstX = stickX;
        dstY = stickY;

        return stickX != 0.f || stickY != 0.f;
    }

    bool setTriggerRatio(float& dst, float trigger) {
        if (trigger > 0.f) {
            dst = trigger;
            return true;
        } else if (dst > 0.f) {
            dst = 0.f;
            return true;
        }

        return false;
    }

    std::string getDeviceName(IGameInputDevice* pDevice) {
        const GameInputDeviceInfo *pInfo = pDevice->GetDeviceInfo();
        if (const GameInputString *pName = pInfo->displayName; pName != nullptr) {
            return std::string{ pName->data, pName->data + pName->sizeInBytes };
        }

        APP_LOCAL_DEVICE_ID deviceId = pInfo->deviceId;
        std::string str;
        for (size_t i = 0; i < sizeof(deviceId.value); i++) {
            str += std::format("{:02X}", deviceId.value[i]);
        }
        return str;
    }
}

GameInput::GameInput() : ISource(DeviceType::eGameInput) {
    HR_CHECK(GameInputCreate(&pInput));
}

GameInput::~GameInput() {
    if (pDevice != nullptr) {
        pDevice->Release();
    }

    pInput->Release();
}

ReportOnce getStateError;
bool GameInput::poll(State& state) {
    getStateError([] {
        simcoe::logInfo("polling gamepad state");
    });

    GameInputGamepadState gamepadState = {};
    ComPtr<IGameInputReading> pReading = nullptr;
    if (HRESULT hr = pInput->GetCurrentReading(GameInputKindGamepad, pDevice, &pReading); SUCCEEDED(hr)) {
        if (pDevice == nullptr) {
            pReading->GetDevice(&pDevice);

            simcoe::logInfo("gamepad device found {}", getDeviceName(pDevice));
            buttonPressIndex = 1;
            getStateError.reset();
        }

        if (!pReading->GetGamepadState(&gamepadState)) {
            getStateError([]{
                simcoe::logError("failed to get gamepad state");
            });
            return false;
        }
    } else if (pDevice != nullptr) {
        simcoe::logError("gamepad device lost");

        pDevice->Release();
        pDevice = nullptr;
        return false;
    }

    bool bDirty = false;
    for (const auto& [button, mask] : kGamepadButtons) {
        bDirty |= updateButton(state, button, gamepadState.buttons, mask);
    }

    bDirty |= setStickAxis(state.axes[Axis::eGamepadLeftX], state.axes[Axis::eGamepadLeftY], gamepadState.leftThumbstickX, gamepadState.leftThumbstickY);
    bDirty |= setStickAxis(state.axes[Axis::eGamepadRightX], state.axes[Axis::eGamepadRightY], gamepadState.rightThumbstickX, gamepadState.rightThumbstickY);

    bDirty |= setTriggerRatio(state.axes[Axis::eGamepadLeftTrigger], gamepadState.leftTrigger);
    bDirty |= setTriggerRatio(state.axes[Axis::eGamepadRightTrigger], gamepadState.rightTrigger);

    if (bDirty) {
        simcoe::logInfo("gamepad: {}", getDeviceName(pDevice));
    }

    return bDirty;
}

bool GameInput::updateButton(State& state, Button button, GameInputGamepadButtons input, GameInputGamepadButtons mask) {
    bool bUpdate = (input & mask) != 0;
    if (!bUpdate) {
        return detail::update(state.buttons[button], size_t(0));
    }

    if (state.buttons[button] != 0) {
        return false;
    }

    return detail::update(state.buttons[button], buttonPressIndex++);
}
