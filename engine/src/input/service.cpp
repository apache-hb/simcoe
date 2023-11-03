#include "engine/input/service.h"
#include "engine/log/service.h"

#include "engine/input/win32-device.h"
#include "engine/input/xinput-device.h"

#include "engine/config/ext/builder.h"

using namespace simcoe;

// freetype

InputService::InputService() {
    CFG_DECLARE("input",
        CFG_FIELD_INT("interval", &pollInterval),
        CFG_FIELD_TABLE("mouse",
            CFG_FIELD_BOOL("enable", &bEnableMouse),
            CFG_FIELD_BOOL("capture", &bLockMouse)
        ),
        CFG_FIELD_BOOL("keyboard", &bEnableKeyboard),
        CFG_FIELD_TABLE("xinput",
            CFG_FIELD_BOOL("gamepad0", &bEnableGamepad0)
        )
    );
}

bool InputService::createService() {
    if (bEnableKeyboard) {
        addSource(new input::Win32Keyboard());
    }

    if (bEnableMouse) {
        addSource(new input::Win32Mouse(PlatformService::getWindow(), bLockMouse));
    }

    if (bEnableGamepad0) {
        addSource(new input::XInputGamepad(0));
    }

    pThread = ThreadService::newThread(threads::eResponsive, "input", [this](std::stop_token stop) {
        auto interval = std::chrono::milliseconds(pollInterval);

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
