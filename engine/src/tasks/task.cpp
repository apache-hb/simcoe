#include "engine/tasks/task.h"

#include "engine/os/system.h"
#include "engine/engine.h"

using namespace simcoe;
using namespace simcoe::tasks;

std::jthread WorkThread::start(const char *name) {
    return std::jthread([this, name](std::stop_token token) {
        simcoe::logInfo("thread `{}` started", name);

        setThreadName(name);
        threadLock.migrate();

        this->run(token);

        // drain the queue
        while (process()) { }

        simcoe::logInfo("thread `{}` stopped", name);
    });
}
