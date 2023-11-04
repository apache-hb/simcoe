#include "engine/threads/queue.h"

#include "engine/threads/service.h"

#include "engine/service/debug.h"
#include "engine/log/service.h"

using namespace simcoe;
using namespace simcoe::threads;

// non-blocking

bool WorkQueue::tryGetMessage() {
    if (workQueue.try_dequeue(message)) {
        message.item();
        return true;
    }

    return false;
}

// blocking

bool BlockingWorkQueue::tryGetMessage() {
    if (workQueue.try_dequeue(message)) {
        message.item();
        return true;
    }

    return false;
}

void BlockingWorkQueue::waitForMessage() {
    workQueue.wait_dequeue(message);
    message.item();
}

bool BlockingWorkQueue::process(std::chrono::milliseconds timeout) {
    if (tryGetMessage(message, timeout)) {
        message.item();
        return true;
    }

    return false;
}

bool BlockingWorkQueue::tryGetMessage(WorkMessage& dst, std::chrono::milliseconds timeout) {
    return workQueue.wait_dequeue_timed(dst, timeout);
}
