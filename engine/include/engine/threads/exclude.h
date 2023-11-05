#pragma once

#include "engine/service/debug.h"
#include "engine/threads/service.h"

namespace simcoe::threads {
    struct ThreadExclusiveRegion {
        ThreadExclusiveRegion(
            ThreadId expectedId = ThreadService::getCurrentThreadId(),
            std::string_view expectedName = threads::getThreadName()
        );

        void migrate(
            ThreadId expectedId = ThreadService::getCurrentThreadId(),
            std::string_view expectedName = threads::getThreadName()
        );

        void verify(std::string_view detail = "");

        std::string_view getExpectedThreadName() const { return expectedThreadName; }

    private:
        ThreadId expectedThreadId;
        std::string_view expectedThreadName;
    };
}
