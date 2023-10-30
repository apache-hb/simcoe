#pragma once

#include "engine/service/debug.h"
#include "engine/threads/service.h"

namespace simcoe::threads {
    struct ThreadExclusiveRegion {
        ThreadExclusiveRegion(
            ThreadId expectedId = ThreadService::getCurrentThreadId(),
            std::string expectedName = DebugService::getThreadName()
        );

        void migrate(
            ThreadId expectedId = ThreadService::getCurrentThreadId(),
            std::string expectedName = DebugService::getThreadName()
        );

        void verify(std::string_view detail = "");

        std::string_view getExpectedThreadName() const { return expectedThreadName; }

    private:
        ThreadId expectedThreadId;
        std::string expectedThreadName;
    };
}
