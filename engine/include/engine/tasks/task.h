#pragma once

#include <functional>
#include <string>
#include <thread>

#include "engine/tasks/exclude.h"

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
            threadLock.verify();

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

        WorkMessage message;
        moodycamel::ConcurrentQueue<WorkMessage> workQueue{64};

    protected:
        ThreadLock threadLock;
    };

    struct WorkThread : WorkQueue {
        WorkThread(size_t size, const char *name)
            : WorkQueue(size)
            , workThread(start(name))
        { }

        virtual ~WorkThread() = default;

        virtual void run(std::stop_token token) {
            while (!token.stop_requested()) {
                process();
            }
        }

        void stop() {
            workThread.request_stop();
            workThread.join();
        }

    private:
        std::jthread start(const char *name);

        std::jthread workThread;
    };
}
