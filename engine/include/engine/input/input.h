#pragma once

namespace simcoe::input {
    enum DeviceType {
        eDeviceRawInput, ///< Windows Raw Input
        eDeviceWin32,    ///< win32 messages
        eDeviceXInput,   ///< XInput
        eDeviceGameInput ///< GameInput
    };

    struct ISource {

    };

    struct IListener {

    };

    struct IManager {

    };
}
