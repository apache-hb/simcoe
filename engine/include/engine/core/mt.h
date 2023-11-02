#pragma once

#include <mutex>
#include <shared_mutex>

// thread safe range based for loops
namespace simcoe::mt {
    using shared_mutex = std::shared_mutex;
    using write_lock = std::unique_lock<shared_mutex>;
    using read_lock = std::shared_lock<shared_mutex>;

    template<typename M, typename T>
    struct Iterator {
        Iterator(M& mutex, T& container)
            : mutex(mutex)
            , container(container)
        {
            mutex.lock();
        }

        ~Iterator() {
            mutex.unlock();
        }

        auto begin() { return container.begin(); }
        auto end() { return container.end(); }

        auto begin() const { return container.begin(); }
        auto end() const { return container.end(); }

    private:
        M& mutex;
        T& container;
    };

    // readonly iterator
    template<typename M, typename T>
    Iterator<M, T> roIter(M& mutex, const T& container) {
        return Iterator<M, T>(mutex, container.begin());
    }

    // readwrite iterator
    template<typename M, typename T>
    Iterator<M, T> rwIter(M& mutex, T& container) {
        return Iterator<M, T>(mutex, container.begin());
    }
}
