#pragma once

#include "engine/core/macros.h"

#include "vendor/moodycamel/concurrent.h"

namespace simcoe::mt {
    template<typename T, template<typename...> typename TQueue> 
    struct BaseMessageQueue {
        SM_NOCOPY(BaseMessageQueue)

        using QueueType = TQueue<T>;

        BaseMessageQueue(size_t size)
            : queue(size)
        { }

        /**
         * @brief add an item to the queue
         * 
         * @param item the item to add
         * @return true 
         * @return false 
         */
        bool tryEnqueue(T&& item) {
            return queue.try_enqueue(std::forward<T>(item));
        }

        void enqueue(T&& item) {
            SM_ASSERT(queue.enqueue(std::forward<T>(item)));
        }

    protected:
        QueueType queue;
    };

    template<typename T>
    struct AsyncMessageQueue : public BaseMessageQueue<T, moodycamel::ConcurrentQueue> {
        using Super = BaseMessageQueue<T, moodycamel::ConcurrentQueue>;
        using Super::Super;

        bool tryGetMessage(T& dst) {
            return Super::queue.try_dequeue(dst);
        }
    };

    template<typename T>
    struct BlockingMessageQueue : BaseMessageQueue<T, moodycamel::BlockingConcurrentQueue> {
        using Super = BaseMessageQueue<T, moodycamel::BlockingConcurrentQueue>;
        using Super::Super;

        bool tryGetMessage(T& dst) {
            return Super::queue.try_dequeue(dst);
        }

        template<typename Rep, typename Period>
        bool tryGetMessage(T& dst, std::chrono::duration<Rep, Period> timeout) {
            return Super::queue.wait_dequeue_timed(dst, timeout);
        }

        template<typename Rep, typename Period>
        size_t tryGetBulk(T *dst, size_t count, std::chrono::duration<Rep, Period> timeout) {
            return Super::queue.wait_dequeue_bulk_timed(dst, count, timeout);
        }
    };
}
