#include "engine/input/gameinput-device.h"

#include "engine/os/system.h"

using namespace simcoe;
using namespace simcoe::input;

#define HR_CHECK(expr) \
    do { \
        if (HRESULT hr = (expr); FAILED(hr)) { \
            simcoe::logError("failure: {} ({})", #expr, simcoe::getErrorName(hr)); \
            throw std::runtime_error(#expr); \
        } \
    } while (false)

GameInput::GameInput() : ISource(DeviceType::eGameInput) {
    HR_CHECK(GameInputCreate(&pInput));
}

GameInput::~GameInput() {
    pInput->Release();
}

bool GameInput::poll(State& state) {
    IGameInputReading *pReading = nullptr;
    if (HRESULT hr = pInput->GetCurrentReading(GameInputKindGamepad, pDevice, &pReading); SUCCEEDED(hr)) {
        if (pDevice == nullptr) {
            pReading->GetDevice(&pDevice);
        }

        GameInputGamepadState gamepadState;
        if (!pReading->GetGamepadState(&gamepadState)) {
            simcoe::logError("failed to get gamepad state");
        }
        pReading->Release();
        simcoe::logInfo("state retrieved");

        return true;

    } else if (pDevice != nullptr) {
        pDevice->Release();
        pDevice = nullptr;
    }

    return false;
}
