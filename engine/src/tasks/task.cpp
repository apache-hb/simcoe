#include "engine/tasks/task.h"

#include "engine/service/debug.h"
#include "engine/service/logging.h"

using namespace simcoe;
using namespace simcoe::tasks;

std::jthread WorkThread::start(std::string_view name) {
    return std::jthread([this, name](std::stop_token token) {
        DebugService::setThreadName(name);
        LOG_INFO("thread `{}` started", name);

        this->run(token);

        // drain the queue
        while (process()) { }

        LOG_INFO("thread `{}` stopped", name);
    });
}
