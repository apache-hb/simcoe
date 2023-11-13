#include "engine/input/service.h"
#include "engine/log/service.h"

#include "engine/input/win32-device.h"
#include "engine/input/xinput-device.h"

#include "engine/config/system.h"

using namespace simcoe;

// internal data

config::ConfigValue<int64_t> cfgPollInterval("input", "poll_interval", "How often to poll input devices (in us)", 500);

config::ConfigValue<bool> cfgEnableKeyboard("input", "keyboard_enable", "Enable keyboard input", true);
config::ConfigValue<bool> cfgEnableMouse("input/mouse", "enable", "Enable mouse input", true);
config::ConfigValue<bool> cfgLockMouse("input/mouse", "capture_input", "Lock mouse to window", false);

config::ConfigValue<bool> cfgEnableGamepad0("input/xinput", "gamepad0_enable", "Enable xinput gamepad0", true);

namespace {
    mt::SharedMutex gMutex{"input"};
    input::Manager gManager;
    threads::ThreadHandle *pThread = nullptr;

    input::Win32Keyboard *pKeyboard = nullptr;
    input::Win32Mouse *pMouse = nullptr;
    input::XInputGamepad *pGamepad0 = nullptr;
}

// service api

bool InputService::createService() {
    if (cfgEnableKeyboard.getCurrentValue()) {
        pKeyboard = new input::Win32Keyboard();
        addSource(pKeyboard);
    }

    if (cfgEnableMouse.getCurrentValue()) {
        pMouse = new input::Win32Mouse(PlatformService::getWindow(), cfgLockMouse.getCurrentValue());
        addSource(pMouse);
    }

    if (cfgEnableGamepad0.getCurrentValue()) {
        pGamepad0 = new input::XInputGamepad(0);
        addSource(pGamepad0);
    }

    pThread = ThreadService::newThread(threads::eResponsive, "input", [](std::stop_token stop) {
        auto interval = std::chrono::microseconds(cfgPollInterval.getCurrentValue());

        while (!stop.stop_requested()) {
            pollInput();

            std::this_thread::sleep_for(interval);
        }
    });
    return true;
}

void InputService::destroyService() {

}

void InputService::addSource(input::ISource *pSource) {
    mt::WriteLock lock(getMutex());
    getManager().addSource(pSource);
}

void InputService::addClient(input::IClient *pClient) {
    mt::WriteLock lock(getMutex());
    getManager().addClient(pClient);
}

void InputService::pollInput() {
    mt::ReadLock lock(getMutex());
    getManager().poll();
}

void InputService::handleMsg(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    mt::ReadLock lock(getMutex());

    if (pKeyboard) {
        pKeyboard->handleMsg(uMsg, wParam, lParam);
    }
}

mt::SharedMutex &InputService::getMutex() { return gMutex; }
input::Manager &InputService::getManager() { return gManager; }