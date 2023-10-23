#pragma once

#include <thread>

#include "engine/os/system.h"

namespace simcoe {
    struct ThreadExclusiveRegion {
        ThreadExclusiveRegion(
            std::thread::id expectedId = std::this_thread::get_id(),
            std::string expectedName = simcoe::getThreadName()
        );

        void migrate(
            std::thread::id expectedId = std::this_thread::get_id(),
            std::string expectedName = simcoe::getThreadName()
        );

        void verify(std::string_view info = "");

        std::string_view getExpectedThreadName() const { return expectedThreadName; }

    private:
        std::thread::id expectedThreadId;
        std::string expectedThreadName;
    };
}
