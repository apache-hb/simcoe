#pragma once

// thread safe range based for loops
namespace simcoe::mt {
    template<typename M, typename T>
    struct Iterator {
        Iterator(M& mutex, const T& container)
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
        const T& container;
    };

    // readonly iterator
    template<typename M, typename T>
    Iterator<M, T> roIter(M& mutex, const T& container) {
        return Iterator<M, T>(mutex, container);
    }

    // readwrite iterator
    template<typename M, typename T>
    Iterator<M, T> rwIter(M& mutex, T& container) {
        return Iterator<M, T>(mutex, container);
    }
}
