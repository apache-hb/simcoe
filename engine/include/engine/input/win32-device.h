#pragma once

#include "engine/input/input.h"

namespace simcoe::input {
    struct Win32Keyboard final : ISource {
        Win32Keyboard();
    };

    struct Win32Mouse final : ISource {
        Win32Mouse();
    };
}
