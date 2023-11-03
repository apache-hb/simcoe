#include "engine/threads/queue.h"

#include "engine/threads/service.h"

#include "engine/service/debug.h"
#include "engine/log/service.h"

using namespace simcoe;
using namespace simcoe::threads;

WorkQueue::WorkQueue(size_t size)
    : workQueue(size)
{ }

void WorkQueue::add(std::string name, WorkItem item) {
    WorkMessage event = {
        .name = name,
        .item = item
    };

    workQueue.enqueue(event);
}

bool WorkQueue::process() {
    if (workQueue.try_dequeue(message)) {
        message.item();
        return true;
    }

    return false;
}
