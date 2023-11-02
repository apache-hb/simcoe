#pragma once

#include <functional>
#include <string>

#include "engine/core/unique.h"

#include "engine/threads/thread.h"

#include "moodycamel/blockingconcurrentqueue.h"

namespace simcoe::threads {
    using WorkItem = std::function<void()>;

    struct WorkQueue {
        WorkQueue(size_t size);

        void add(std::string name, WorkItem item);
        bool process();

    private:
        struct WorkMessage {
            std::string name;
            WorkItem item;
        };

        WorkMessage message;
        moodycamel::ConcurrentQueue<WorkMessage> workQueue;
    };
}
