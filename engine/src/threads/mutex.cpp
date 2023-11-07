#include "engine/threads/mutex.h"
#include "engine/core/error.h"

#include "engine/threads/service.h"

#include <simcoe-config.h>

using namespace simcoe;
using namespace simcoe::mt;

Mutex::Mutex([[maybe_unused]] std::string name)
#if DEBUG_ENGINE
    : name(std::move(name))
    , owner(0)
#endif
{ }

#if DEBUG_ENGINE
#   define DEBUG_TRY(...) try { __VA_ARGS__ }
#   define DEBUG_CATCH(err, ...) catch (err) { __VA_ARGS__ }
#else
#   define DEBUG_TRY(...) __VA_ARGS__
#   define DEBUG_CATCH(err, ...)
#endif

void Mutex::lock() {
#if DEBUG_ENGINE
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
#if DEBUG_ENGINE
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
#if DEBUG_ENGINE
    owner = 0;
#endif
}
