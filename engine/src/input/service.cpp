#include "engine/input/service.h"
#include "engine/log/service.h"

#include "engine/input/win32-device.h"
#include "engine/input/xinput-device.h"

#include "engine/config/system.h"

using namespace simcoe;

// internal data

config::ConfigValue<int64_t> cfgPollInterval("input", "poll-interval", "How often to poll input devices (in ms)", 16);

config::ConfigValue<bool> cfgEnableKeyboard("input", "keyboardenable", "Enable keyboard input", true);
config::ConfigValue<bool> cfgEnableMouse("input", "mouseenable", "Enable mouse input", true);
config::ConfigValue<bool> cfgLockMouse("input", "mousecapture", "Lock mouse to window", false);

config::ConfigValue<bool> cfgEnableGamepad0("input/xinput", "gamepad0", "Enable xinput gamepad0", true);

// service api

InputService::InputService() {

}

bool InputService::createService() {
    if (cfgEnableKeyboard.getValue()) {
        addSource(new input::Win32Keyboard());
    }

    if (cfgEnableMouse.getValue()) {
        addSource(new input::Win32Mouse(PlatformService::getWindow(), cfgLockMouse.getValue()));
    }

    if (cfgEnableGamepad0.getValue()) {
        addSource(new input::XInputGamepad(0));
    }

    pThread = ThreadService::newThread(threads::eResponsive, "input", [](std::stop_token stop) {
        auto interval = std::chrono::milliseconds(cfgPollInterval.getValue());

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
    mt::write_lock lock(getMutex());
    getManager().addSource(pSource);
}

void InputService::addClient(input::IClient *pClient) {
    mt::write_lock lock(getMutex());
    getManager().addClient(pClient);
}
