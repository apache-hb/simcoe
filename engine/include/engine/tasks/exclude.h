#pragma once

#include <thread>

#include "engine/system/system.h"

namespace simcoe::tasks {
    struct ThreadExclusiveRegion {
        ThreadExclusiveRegion(
            std::thread::id expectedId = std::this_thread::get_id(),
            std::string expectedName = system::getThreadName()
        );

        void migrate(
            std::thread::id expectedId = std::this_thread::get_id(),
            std::string expectedName = system::getThreadName()
        );

        void verify(std::string_view info = "");

        std::string_view getExpectedThreadName() const { return expectedThreadName; }

    private:
        std::thread::id expectedThreadId;
        std::string expectedThreadName;
    };
}
