#include "engine/input/gameinput-service.h"

#include "wrl.h"

using namespace simcoe;
using namespace simcoe::input;

using Microsoft::WRL::ComPtr;

GameInputService::GameInputService(input::Manager *pManager) : pManager(pManager) {
    if (FAILED(GameInputCreate(&pInstance))) {
        throw std::runtime_error("failed to create game input instance");
    }

    HRESULT hr = pInstance->RegisterDeviceCallback(
        /* device = */ nullptr,
        /* inputKind = */ GameInputKindGamepad | GameInputKindMouse | GameInputKindKeyboard,
        /* statusFilter = */ GameInputDeviceConnected | GameInputDeviceNoStatus,
        /* enumerationKind =*/ GameInputAsyncEnumeration,
        /* context = */ this,
        /* callbackFunc = */ GameInputService::onDeviceEvent,
        /* callbackToken = */ &eventToken
    );

    if (FAILED(hr)) {
        throw std::runtime_error("failed to register game input callback");
    }
}

GameInputService::~GameInputService() {
    for (auto& [pDevice, pSource] : loadedDevices) {
        removeDevice(pDevice);
    }

    pInstance->Release();
}

void GameInputService::addDevice(IGameInputDevice *pDevice) {
    loadedDevices.emplace(pDevice, new Device(pManager, pDevice));
}

void GameInputService::removeDevice(IGameInputDevice *pDevice) {
    auto it = loadedDevices.find(pDevice);
    if (it == loadedDevices.end()) {
        return;
    }

    delete it->second; // TODO: should use unique_ptr but msvc is being a bitch and giving me
                       //       massive template errors i just cant be asked to figure out
    loadedDevices.erase(it);
}

void GameInputService::onDeviceEvent(
    GameInputCallbackToken token,
    void *pContext,
    IGameInputDevice *pDevice,
    uint64_t timestamp,
    GameInputDeviceStatus currentStatus,
    GameInputDeviceStatus previousStatus)
{
    // TODO: handle aggregating multiple keyboards into one
    //       some keyboards report as up to 4 keyboards
    auto pService = static_cast<GameInputService*>(pContext);

    if (currentStatus & GameInputDeviceConnected) {
        pService->addDevice(pDevice);
    } else {
        pService->removeDevice(pDevice);
    }
}

GameInputService::Device::Device(input::Manager *pManager, IGameInputDevice *pDevice)
    : ISource(input::DeviceType::eGameInput)
    , pManager(pManager)
    , pDevice(pDevice)
{
    pManager->addSource(this);
}

GameInputService::Device::~Device() {
    pManager->removeSource(this);
    pDevice->Release();
}
