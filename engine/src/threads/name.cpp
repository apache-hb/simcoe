#include "engine/threads/service.h"

using namespace simcoe;

namespace {
    // this grows memory fungus by design
    // if any value in here is changed after its created then
    // getThreadName can return invalid data
    // we want to return a string_view to avoid memory allocations inside the logger
    mt::shared_mutex lock;
    std::unordered_map<threads::ThreadId, std::string> names;
}

void threads::setThreadName(std::string name, ThreadId id) {
    mt::write_lock guard(lock);
    names.emplace(id, name);
}

std::string_view threads::getThreadName(ThreadId id) {
    mt::read_lock guard(lock);
    if (auto it = names.find(id); it != names.end()) {
        return it->second;
    }

    return "";
}

threads::ThreadId threads::getCurrentThreadId() {
    return ::GetCurrentThreadId();
}
