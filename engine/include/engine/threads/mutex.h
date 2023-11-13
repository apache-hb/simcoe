#pragma once

#include "engine/core/macros.h"

#include "engine/profile/profile.h"

#include "engine/threads/thread.h"

#include <string>

#include <mutex>
#include <shared_mutex>

namespace simcoe::mt {
    struct BaseMutex {
        SM_NOCOPY(BaseMutex)

        BaseMutex(std::string name);
        
        BaseMutex(std::string_view name)
            : BaseMutex(std::string(name))
        { }

        BaseMutex(const char *pzName)
            : BaseMutex(std::string(pzName))
        { }
        
    protected:
        void verifyOwner();
        void resetOwner();

#if SM_DEBUG_THREADS
        std::string_view getName() const { return name; }
        threads::ThreadId getOwner() const { return owner; }
#endif

    private:
#if SM_DEBUG_THREADS
        std::string name; /// the name of the mutex
        threads::ThreadId owner; /// the thread that currently owns the mutex
#endif
    };

    struct Mutex : public BaseMutex {
        using BaseMutex::BaseMutex;

        // std::mutex interface
        void lock();
        bool try_lock();
        void unlock();

        std::mutex& getInner() { return mutex; }

    private:
        std::mutex mutex;
    };

    struct SharedMutex : public BaseMutex {
        using BaseMutex::BaseMutex;

        // std::shared_mutex interface
        void lock();
        void unlock();

        void lock_shared();
        void unlock_shared();

        std::shared_mutex& getInner() { return mutex; }

    private:
        std::shared_mutex mutex;
    };

    using WriteLock = std::unique_lock<SharedMutex>;
    using ReadLock = std::shared_lock<SharedMutex>;

    // TODO: implement this rather than having `getInner` functions
    // struct ConditionVariable {

    // };
}
