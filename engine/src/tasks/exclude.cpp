#include "engine/tasks/exclude.h"

using namespace simcoe;

ThreadExclusiveRegion::ThreadExclusiveRegion(std::thread::id expectedId, std::string expectedName) {
    migrate(expectedId, expectedName);
}

void ThreadExclusiveRegion::migrate(std::thread::id expectedId, std::string expectedName) {
    expectedThreadId = expectedId;
    expectedThreadName = expectedName;
}

void ThreadExclusiveRegion::verify(std::string_view info) {
    std::thread::id currentThreadId = std::this_thread::get_id();
    ASSERTF(currentThreadId == expectedThreadId, "thread migration detected: locked to {}, visited by {} (info: {})", expectedThreadName, simcoe::getThreadName(), info);
}
