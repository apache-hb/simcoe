#include "engine/tasks/exclude.h"

#include "engine/core/panic.h"

using namespace simcoe;
using namespace simcoe::tasks;

ThreadExclusiveRegion::ThreadExclusiveRegion(std::thread::id expectedId, std::string expectedName) {
    migrate(expectedId, expectedName);
}

void ThreadExclusiveRegion::migrate(std::thread::id expectedId, std::string expectedName) {
    expectedThreadId = expectedId;
    expectedThreadName = expectedName;
}

void ThreadExclusiveRegion::verify(std::string_view detail) {
    std::thread::id currentThreadId = std::this_thread::get_id();
    ASSERTF(currentThreadId == expectedThreadId, "thread migration detected: locked to {}, visited by {} (info: {})", expectedThreadName, DebugService::getThreadName(), detail);
}
