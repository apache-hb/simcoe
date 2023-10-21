#include "engine/tasks/exclude.h"

using namespace simcoe;
using namespace simcoe::tasks;

ThreadLock::ThreadLock(std::thread::id expectedThreadId, std::string expectedThreadName) {
    migrate(expectedThreadId, expectedThreadName);
}

void ThreadLock::migrate(std::thread::id newId, std::string newName) {
    expectedThreadId = newId;
    expectedThreadName = newName;
}

void ThreadLock::verify() const {
    if (expectedThreadId != std::this_thread::get_id()) {
        auto name = simcoe::getThreadName();
        simcoe::logError("ThreadLock: thread {} got visited by thread {}", expectedThreadName, name);
        simcoe::printBacktrace();
        throw std::runtime_error("ThreadLock: thread migration detected");
    }
}
