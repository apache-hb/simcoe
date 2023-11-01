#pragma once

#include <functional>
#include <string>

#include "engine/threads/service.h"
#include "engine/threads/schedule.h"

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
        moodycamel::BlockingConcurrentQueue<WorkMessage> workQueue;
    };

    struct WorkThread : WorkQueue {
        WorkThread(size_t size, std::string_view name, Scheduler *pScheduler)
            : WorkQueue(size)
            , workThread(start(name, pScheduler))
        { }

        virtual ~WorkThread() = default;

        virtual void run(std::stop_token token) {
            while (!token.stop_requested()) {
                process();
            }
        }

    private:
        threads::Thread& start(std::string_view name, Scheduler *pScheduler);

        // TODO: this should be a handle into the scheduler
        threads::Thread& workThread;
    };
}
