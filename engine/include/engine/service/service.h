#pragma once

#include "engine/core/panic.h"

#include <span>
#include <string_view>
#include <atomic>

namespace simcoe {
    using NameSpan = std::span<const std::string_view>;

    enum ServiceState {
        eServiceInitial = (1 << 0), // service has not been setup yet
        eServiceSetup   = (1 << 1), // service has been setup
        eServiceCreated = (1 << 2), // service has been created
        eServiceFaulted = (1 << 3) // service has been created but failed to initialize
    };

    struct IService {
        virtual ~IService() = default;

        virtual std::string_view getName() const = 0;
        virtual NameSpan getDeps() const = 0;

        void create();
        void destroy();

    protected:
        virtual bool createService() = 0;
        virtual void destroyService() = 0;

        ServiceState getState() const { return state; }

        void ensureState(ServiceState validStates, const char *fn) const {
            ASSERTF(state & validStates, "service {} not in valid state, cannot call {}", getName(), fn);
        }

    private:
        std::atomic<ServiceState> state = eServiceInitial;
    };

    template<typename T>
    struct IStaticService : IService {
        using IService::IService;

        std::string_view getName() const override { return T::kServiceName; }
        NameSpan getDeps() const override { return T::kServiceDeps; }

        static T *get() {
            static T instance;
            return &instance;
        }

        static IService *service() {
            return static_cast<IService*>(get());
        }

        static T *use(ServiceState validStates, const char *fn) {
            T *pService = get();
            pService->ensureState(validStates, fn);
            return pService;
        }

        static ServiceState getState() {
            return get()->IService::getState();
        }

        static void ensureState(ServiceState validStates, const char *fn) {
            get()->IService::ensureState(validStates, fn);
        }
    };

    struct ServiceRuntime {
        ServiceRuntime(std::span<IService*> services);
        ~ServiceRuntime();

    private:
        std::span<IService*> services;
    };
}

#define USE_SERVICE(state, fn) \
    use(ServiceState(state), #fn)->fn

#define ENSURE_STATE(state) \
    ensureState(ServiceState(state), __func__)

/// services are a little verbose but they work well enough
/// all methods should have both a static and non-static version, like so

/// public: static void foo() { USE_SERVICE(eServiceCreated, doFoo)(); }
/// private: void doFoo();

/// and for fields, they should be private and accessed through a static getter

/// public: static int getBar() { return USE_SERVICE(eServiceCreated, bar); }
/// private: int bar = 0;
