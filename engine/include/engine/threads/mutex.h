#pragma once

#include "engine/core/macros.h"

#include "engine/threads/thread.h"

#include <string>

#include <mutex>
#include <shared_mutex>

namespace simcoe::mt {
    struct BaseMutex {
        SM_NOCOPY(BaseMutex)

    protected:
        BaseMutex(std::string_view name);

        void verifyOwner();
        void resetOwner();

#if SM_DEBUG_THREADS
        std::string_view getName() const { return name; }
#endif

    private:
#if SM_DEBUG_THREADS
        std::string_view name; /// the name of the mutex
        threads::ThreadId owner; /// the thread that currently owns the mutex
#endif
    };

    struct Mutex : BaseMutex {
        Mutex(std::string_view name)
            : BaseMutex(name)
        { }

        // std::mutex interface
        void lock();
        bool try_lock();
        void unlock();

    private:
        std::mutex mutex;
    };

    struct SharedMutex : public BaseMutex {
        SharedMutex(std::string_view name)
            : BaseMutex(name)
        { }

        // std::shared_mutex interface
        void lock();
        void unlock();

        void lock_shared();
        void unlock_shared();

    private:
        std::shared_mutex mutex;
    };

    using WriteLock = std::unique_lock<SharedMutex>;
    using ReadLock = std::shared_lock<SharedMutex>;
}
