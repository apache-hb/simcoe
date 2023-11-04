#pragma once

#include <functional>
#include <string>

#include "engine/core/unique.h"

#include "engine/threads/thread.h"

#include "moodycamel/blockingconcurrentqueue.h"

namespace simcoe::threads {
    using WorkItem = std::function<void()>;

    struct WorkMessage {
        std::string name;
        WorkItem item;
    };

    template<template<class...> typename TQueue>
    struct BaseWorkQueue {
        BaseWorkQueue(size_t size)
            : workQueue(size)
        { }

        void add(std::string name, WorkItem item) {
            workQueue.enqueue({ name, item });
        }

    protected:
        WorkMessage message;
        TQueue<WorkMessage> workQueue;
    };

    /// !IMPORTANT! these queues are thread safe to emplace into to, but not to remove from

    struct WorkQueue : public BaseWorkQueue<moodycamel::ConcurrentQueue> {
        using Super = BaseWorkQueue<moodycamel::ConcurrentQueue>;
        using Super::Super;

        // try and process a message immediately
        bool tryGetMessage();
    };

    struct BlockingWorkQueue : public BaseWorkQueue<moodycamel::BlockingConcurrentQueue> {
        using Super = BaseWorkQueue<moodycamel::BlockingConcurrentQueue>;
        using Super::Super;

        // try and process a message immediately
        bool tryGetMessage();

        // be quite careful with blocking forever
        // deadlocks during shutdown are possible
        void waitForMessage();

        // returns true if message was processed
        bool process(std::chrono::milliseconds timeout);

        // this is thread safe to call from any thread
        // assuming @param dst is synchronized externally
        // process a message using a provided destination
        bool tryGetMessage(WorkMessage& dst, std::chrono::milliseconds timeout);
    };
}
