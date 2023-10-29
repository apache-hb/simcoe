#pragma once

#include <thread>

#include "engine/service/debug.h"

namespace simcoe::threads {
    struct ThreadExclusiveRegion {
        ThreadExclusiveRegion(
            std::thread::id expectedId = std::this_thread::get_id(),
            std::string expectedName = DebugService::getThreadName()
        );

        void migrate(
            std::thread::id expectedId = std::this_thread::get_id(),
            std::string expectedName = DebugService::getThreadName()
        );

        void verify(std::string_view detail = "");

        std::string_view getExpectedThreadName() const { return expectedThreadName; }

    private:
        std::thread::id expectedThreadId;
        std::string expectedThreadName;
    };
}
