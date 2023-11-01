#include "engine/threads/queue.h"

#include "engine/service/debug.h"
#include "engine/service/logging.h"

using namespace simcoe;
using namespace simcoe::threads;

WorkQueue::WorkQueue(size_t size)
    : workQueue(size)
{ }

void WorkQueue::add(std::string name, WorkItem item) {
    WorkMessage message = {
        .name = name,
        .item = item
    };

    workQueue.enqueue(message);
}

bool WorkQueue::process() {
    if (workQueue.try_dequeue(message)) {
        message.item();
        return true;
    }

    return false;
}

threads::Thread WorkThread::start(std::string_view name) {
    return [this, name](std::stop_token token) {
        DebugService::setThreadName(name);
        LOG_INFO("thread `{}` started", name);

        this->run(token);

        // drain the queue
        while (process()) { }

        LOG_INFO("thread `{}` stopped", name);
    };
}
