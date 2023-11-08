#include "engine/threads/exclude.h"

#include "engine/core/panic.h"

using namespace simcoe;
using namespace simcoe::threads;

ThreadExclusiveRegion::ThreadExclusiveRegion(ThreadId expectedId, std::string_view expectedName) {
    migrate(expectedId, expectedName);
}

void ThreadExclusiveRegion::migrate(ThreadId expectedId, std::string_view expectedName) {
    expectedThreadId = expectedId;
    expectedThreadName = expectedName;
}

void ThreadExclusiveRegion::verify(std::string_view detail) {
    ThreadId currentThreadId = ThreadService::getCurrentThreadId();
    SM_ASSERTF(currentThreadId == expectedThreadId, "thread migration detected: locked to {}, visited by {} (info: {})", expectedThreadName, threads::getThreadName(), detail);
}
