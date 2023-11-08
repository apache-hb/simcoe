#pragma once

#include <string>

namespace simcoe::audio {
    struct Channel {
        std::string name;
        uint16_t volume = UINT16_MAX; // 0-UINT16_MAX, 0 = muted, UINT16_MAX = max volume
    };

    struct SoundBuffer {

    };
}
