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

    struct InputNameMapping {
        GameInputKind kind;
        std::string_view name;
    };

    constexpr auto kInputKinds = std::to_array<InputNameMapping>({
        { GameInputKindRawDeviceReport  , "RawDeviceReport" },
        { GameInputKindControllerAxis   , "ControllerAxis" },
        { GameInputKindControllerButton , "ControllerButton" },
        { GameInputKindControllerSwitch , "ControllerSwitch" },
        { GameInputKindController       , "Controller" },
        { GameInputKindKeyboard         , "Keyboard" },
        { GameInputKindMouse            , "Mouse" },
        { GameInputKindTouch            , "Touch" },
        { GameInputKindMotion           , "Motion" },
        { GameInputKindArcadeStick      , "ArcadeStick" },
        { GameInputKindFlightStick      , "FlightStick" },
        { GameInputKindGamepad          , "Gamepad" },
        { GameInputKindRacingWheel      , "RacingWheel" },
        { GameInputKindUiNavigation     , "UiNavigation" },
    });

    std::string getInputKind(GameInputKind kind) {
        std::vector<std::string_view> kinds;

        for (const auto& [k, name] : kInputKinds) {
            if ((kind & k) == k) {
                kinds.push_back(name);
            }
        }

        return util::join(kinds, " + ");
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
    ComPtr<IGameInputReading> pReading = nullptr;
    if (HRESULT hr = pInput->GetCurrentReading(GameInputKindGamepad, pDevice, &pReading); SUCCEEDED(hr)) {
        if (pDevice == nullptr) {
            pReading->GetDevice(&pDevice);

            simcoe::logInfo("gameinput device found");
            buttonPressIndex = 1;
            getStateError.reset();
            deviceGeometry.reset();
        }
    } else if (pDevice != nullptr) {
        simcoe::logError("gameinput device lost");

        pDevice->Release();
        pDevice = nullptr;
        return false;
    } else {
        getStateError([]{
            simcoe::logError("failed to get current reading");
        });
        return false;
    }

    GameInputGamepadState padState = {};
    if (!pReading->GetGamepadState(&padState)) {
        getStateError([]{
            simcoe::logError("failed to get gamepad state");
        });
        return false;
    }

    bool bDirty = false;
    for (const auto& [button, mask] : kGamepadButtons) {
        bDirty |= updateButton(state, button, padState.buttons, mask);
    }

    deviceGeometry([&] {
        simcoe::logInfo("reading kind: {}", getInputKind(pReading->GetInputKind()));
    });

    simcoe::logInfo("gamepad");
    simcoe::logInfo("  left stick: ({}, {})", padState.leftThumbstickX, padState.leftThumbstickY);
    simcoe::logInfo("  right stick: ({}, {})", padState.rightThumbstickX, padState.rightThumbstickY);
    simcoe::logInfo("  left trigger: {}", padState.leftTrigger);
    simcoe::logInfo("  right trigger: {}", padState.rightTrigger);

    bDirty |= setStickAxis(state.axes[Axis::eGamepadLeftX], state.axes[Axis::eGamepadLeftY], padState.leftThumbstickX, padState.leftThumbstickY);
    bDirty |= setStickAxis(state.axes[Axis::eGamepadRightX], state.axes[Axis::eGamepadRightY], padState.rightThumbstickX, padState.rightThumbstickY);

    bDirty |= setTriggerRatio(state.axes[Axis::eGamepadLeftTrigger], padState.leftTrigger);
    bDirty |= setTriggerRatio(state.axes[Axis::eGamepadRightTrigger], padState.rightTrigger);

    if (bDirty) {
        simcoe::logInfo("gamepad");
    }

    return bDirty;
}

bool GameInput::updateButton(State& state, Button button, GameInputGamepadButtons input, GameInputGamepadButtons mask) {
    bool bUpdate = (input & mask);
    if (!bUpdate) {
        return detail::update(state.buttons[button], size_t(0));
    }

    if (state.buttons[button] != 0) {
        return false;
    }

    return detail::update(state.buttons[button], buttonPressIndex++);
}
