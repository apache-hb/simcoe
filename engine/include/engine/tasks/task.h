#pragma once

#include <functional>
#include <string>
#include <thread>

#include "moodycamel/concurrentqueue.h"

namespace simcoe::tasks {
    using WorkItem = std::function<void()>;

    struct WorkQueue {
        WorkQueue(size_t size)
            : workQueue(size)
        { }

        void add(std::string name, WorkItem item) {
            WorkMessage message = {
                .name = name,
                .item = item
            };

            workQueue.enqueue(message);
        }

        bool process() {
            WorkMessage message;
            if (workQueue.try_dequeue(message)) {
                message.item();
                return true;
            }

            return false;
        }

    private:
        struct WorkMessage {
            std::string name;
            WorkItem item;
        };

        moodycamel::ConcurrentQueue<WorkMessage> workQueue{64};
    };

    struct WorkThread : WorkQueue {
        WorkThread(size_t size, const char *name)
            : WorkQueue(size)
            , workThread(start(name))
        { }

        virtual ~WorkThread() = default;

        virtual void run(std::stop_token token) = 0;

    private:
        std::jthread start(const char *name);

        std::jthread workThread;
    };
}
