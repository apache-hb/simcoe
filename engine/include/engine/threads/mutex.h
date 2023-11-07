#pragma once

#include "engine/core/macros.h"

#include "engine/threads/thread.h"

#include <string>
#include <mutex>

namespace simcoe::mt {
    struct Mutex {
        SM_NOCOPY(Mutex)

        Mutex(std::string name);

        void lock();
        bool tryLock();
        void unlock();
    private:
#if SM_DEBUG_THREADS
        std::string name; /// the name of the mutex
        threads::ThreadId owner; /// the thread that currently owns the mutex
#endif

        std::mutex mutex;
    };
}
