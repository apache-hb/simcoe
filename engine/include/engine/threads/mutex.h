#pragma once

#include "engine/core/macros.h"

#include "engine/threads/thread.h"

#include <string>
#include <mutex>

#include <simcoe-config.h>

namespace simcoe::mt {
    struct Mutex {
        SM_NOCOPY(Mutex)

        Mutex(std::string name);

        void lock();
        bool tryLock();
        void unlock();
    private:
#if DEBUG_ENGINE
        std::string name; /// the name of the mutex
        threads::ThreadId owner; /// the thread that currently owns the mutex
#endif

        std::mutex mutex;
    };
}
