#pragma once

#include "engine/input/input.h"

namespace simcoe::input::detail {
    template<typename T>
    bool update(T& value, T next) {
        bool bUpdated = value != next;
        value = next;
        return bUpdated;
    }
}
