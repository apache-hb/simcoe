#pragma once

#include "engine/core/unique.h"
#include "engine/core/mt.h"

#include "engine/service/platform.h"

#include "engine/threads/queue.h"
#include "engine/threads/thread.h"

#include <unordered_set>

namespace simcoe {
    // collects thread geometry at startup
    struct ThreadService : IStaticService<ThreadService> {
        ThreadService();

        // IStaticService
        static constexpr std::string_view kServiceName = "threads";
        static constexpr std::array kServiceDeps = { PlatformService::kServiceName };

        // IService
        bool createService() override;
        void destroyService() override;

        // geometry data
        static const threads::Geometry& getGeometry();
        static threads::ThreadId getCurrentThreadId();

        /** talking to the main thread */
        static void enqueueMain(std::string name, threads::WorkItem&& task);
        static void pollMainQueue();

        /** thread naming */

        // TODO: remove thread naming from here, should really be debug only

        /**
         * @brief Set the name of a thread by its id
         *
         * @note this function must be called only once per thread
         *
         * @param name the name of the thread
         * @param id the id of the thread
         */
        static void setThreadName(std::string name, threads::ThreadId id = getCurrentThreadId());

        /**
         * @brief Get the name of a thread by its id
         *
         * @param id the id of the thread
         * @return std::string_view the name of the thread, empty if no name is set or the thread doesnt exist
         */
        static std::string_view getThreadName(threads::ThreadId id = getCurrentThreadId());

        /** worker api */

        static void setWorkerCount(size_t count);
        static size_t getWorkerCount();

        static void enqueueWork(std::string name, threads::WorkItem&& func);

        /** scheduling api */

        /**
         * @brief create a new thread and schedule it
         *
         * @param type the priority of the thread
         * @param name the name of the thread
         * @param start the function to run on the thread
         * @return threads::ThreadHandle* the handle to the thread
         */
        static threads::ThreadHandle *newThread(threads::ThreadType type, std::string_view name, threads::ThreadStart&& start);

        /**
         * @brief create a new job thread and schedule it
         *
         * @param name the name of the job
         * @param delay the delay between each iteration
         * @param step the function to run each iteration
         * @return threads::ThreadHandle* the handle to the thread
         */
        template<typename Rep, typename Period>
        static threads::ThreadHandle *newJob(std::string_view name, std::chrono::duration<Rep, Period> delay, auto&& step) {
            return newThread(threads::eWorker, name, [delay, step](std::stop_token stop) {
                while (!stop.stop_requested()) {
                    step();
                    std::this_thread::sleep_for(delay);
                }
            });
        }

        /**
         * @brief terminate all threads and cleanup
         * @note this leaves dangling pointers to thread handles, we should fix that
         */
        static void shutdown();

    private:
        // this grows memory fungus by design
        // if any value in here is changed after its created then
        // getThreadName can return invalid data
        // we want to return a string_view to avoid memory allocations inside the logger
        mt::shared_mutex threadNameLock;
        std::unordered_map<threads::ThreadId, std::string> threadNames;

        // all currently scheduled threads
        mt::shared_mutex threadHandleLock;
        std::vector<threads::ThreadHandle*> threadHandles;

    public:
        static std::shared_mutex &getPoolLock() { return get()->threadHandleLock; }
        static std::vector<threads::ThreadHandle*> &getPool() { return get()->threadHandles; }
    };
}
