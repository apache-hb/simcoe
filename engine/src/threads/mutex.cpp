#include "engine/threads/mutex.h"
#include "engine/core/error.h"

#include "engine/threads/service.h"

using namespace simcoe;
using namespace simcoe::mt;

BaseMutex::BaseMutex(SM_UNUSED std::string name)
#if SM_DEBUG_THREADS
    : name(name)
    , owner(0)
#endif
{ }

void BaseMutex::verifyOwner() {
#if SM_DEBUG_THREADS
    auto tid = threads::getCurrentThreadId();
    if (owner == tid) {
        core::throwFatal("Mutex '{}' was already locked on this thread", name);
    }
    owner = tid;
#endif
}

void BaseMutex::resetOwner() {
#if SM_DEBUG_THREADS
    owner = 0;
#endif
}

#if SM_DEBUG_THREADS
#   define DEBUG_TRY(...) try { __VA_ARGS__ }
#   define DEBUG_CATCH(err, ...) catch (err) { __VA_ARGS__ }
#else
#   define DEBUG_TRY(...) __VA_ARGS__
#   define DEBUG_CATCH(err, ...)
#endif

// mutex

void Mutex::lock() {
    verifyOwner();

    DEBUG_TRY({
        mutex.lock();
    })
    DEBUG_CATCH(const std::exception& err, {
        core::throwFatal("Failed to lock mutex '{}': {}", getName(), err.what());
    })
}

bool Mutex::try_lock() {
    verifyOwner();

    DEBUG_TRY({
        return mutex.try_lock();
    })
    DEBUG_CATCH(const std::exception& err, {
        core::throwFatal("Failed to try lock mutex '{}': {}", getName(), err.what());
    })
}

void Mutex::unlock() {
    mutex.unlock();
    resetOwner();
}

// shared mutex

void SharedMutex::lock() {
    verifyOwner();

    DEBUG_TRY({
        mutex.lock();
    })
    DEBUG_CATCH(const std::exception& err, {
        core::throwFatal("Failed to lock mutex '{}': {}", getName(), err.what());
    })
}

void SharedMutex::unlock() {
    mutex.unlock();
    resetOwner();
}

void SharedMutex::lock_shared() {
#if SM_DEBUG_THREADS
    auto tid = threads::getCurrentThreadId();
    if (getOwner() == tid) {
        core::throwFatal("Mutex '{}' was already locked on this thread", getName());
    }
#endif
    mutex.lock_shared();
}

void SharedMutex::unlock_shared() {
    mutex.unlock_shared();
}
