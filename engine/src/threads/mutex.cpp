#include "engine/threads/mutex.h"
#include "engine/core/error.h"

#include "engine/threads/service.h"

using namespace simcoe;
using namespace simcoe::mt;

Mutex::Mutex([[maybe_unused]] std::string name)
#if SM_DEBUG_THREADS
    : name(std::move(name))
    , owner(0)
#endif
{ }

#if SM_DEBUG_THREADS
#   define DEBUG_TRY(...) try { __VA_ARGS__ }
#   define DEBUG_CATCH(err, ...) catch (err) { __VA_ARGS__ }
#else
#   define DEBUG_TRY(...) __VA_ARGS__
#   define DEBUG_CATCH(err, ...)
#endif

void Mutex::lock() {
#if SM_DEBUG_THREADS
    if (owner == threads::getCurrentThreadId()) {
        core::throwFatal("Mutex '{}' is already locked by this thread", name);
    }
    owner = threads::getCurrentThreadId();
#endif
    DEBUG_TRY({
        mutex.lock();
    })
    DEBUG_CATCH(const std::exception& err, {
        core::throwFatal("Failed to lock mutex '{}': {}", name, err.what());
    })
}

bool Mutex::tryLock() {
#if SM_DEBUG_THREADS
    if (owner == threads::getCurrentThreadId()) {
        core::throwFatal("Mutex '{}' is already locked by this thread", name);
    }
    owner = threads::getCurrentThreadId();
#endif
    DEBUG_TRY({
        return mutex.try_lock();
    })
    DEBUG_CATCH(const std::exception& err, {
        core::throwFatal("Failed to try lock mutex '{}': {}", name, err.what());
    })
}

void Mutex::unlock() {
    mutex.unlock();
#if SM_DEBUG_THREADS
    owner = 0;
#endif
}
