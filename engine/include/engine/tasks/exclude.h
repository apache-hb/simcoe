#pragma once

#include "engine/os/system.h"

namespace simcoe::tasks {
    /**
     * @brief make sure this point is only ever reached by a single thread of execution
     *        it can be reached multiple times by the same thread.
     */
    struct ThreadLock {
        ThreadLock(
            std::thread::id expectedThreadId = std::this_thread::get_id(),
            std::string expectedThreadName = simcoe::getThreadName()
        );

        void migrate(
            std::thread::id newThreadId = std::this_thread::get_id(),
            std::string newThreadName = simcoe::getThreadName()
        );

        void verify() const;
    private:
        std::thread::id expectedThreadId;
        std::string expectedThreadName;
    };
}
