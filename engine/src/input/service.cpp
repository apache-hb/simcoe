#include "engine/input/service.h"
#include "engine/log/service.h"

#include "engine/input/win32-device.h"
#include "engine/input/xinput-device.h"

#include "engine/config/ext/builder.h"

using namespace simcoe;

// internal data

namespace {
    namespace cfg {
        size_t pollInterval = 16; ///< how often to poll input devices (in ms)

        bool bEnableKeyboard = true; ///< enable keyboard input
        bool bEnableMouse = true; ///< enable mouse input
        bool bLockMouse = false; ///< lock mouse to window

        bool bEnableGamepad0 = true; ///< enable xinput gamepad0
    }
}

// service api

InputService::InputService() {
    CFG_DECLARE("input",
        CFG_FIELD_INT("interval", &cfg::pollInterval),
        CFG_FIELD_TABLE("mouse",
            CFG_FIELD_BOOL("enable", &cfg::bEnableMouse),
            CFG_FIELD_BOOL("capture", &cfg::bLockMouse)
        ),
        CFG_FIELD_BOOL("keyboard", &cfg::bEnableKeyboard),
        CFG_FIELD_TABLE("xinput",
            CFG_FIELD_BOOL("gamepad0", &cfg::bEnableGamepad0)
        )
    );
}

bool InputService::createService() {
    if (cfg::bEnableKeyboard) {
        addSource(new input::Win32Keyboard());
    }

    if (cfg::bEnableMouse) {
        addSource(new input::Win32Mouse(PlatformService::getWindow(), cfg::bLockMouse));
    }

    if (cfg::bEnableGamepad0) {
        addSource(new input::XInputGamepad(0));
    }

    pThread = ThreadService::newThread(threads::eResponsive, "input", [this](std::stop_token stop) {
        auto interval = std::chrono::milliseconds(cfg::pollInterval);

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
