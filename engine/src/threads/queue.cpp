#include "engine/threads/queue.h"

#include "engine/threads/service.h"

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
