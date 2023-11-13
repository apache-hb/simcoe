#pragma once

#include "engine/core/macros.h"
#include "engine/core/panic.h"

#include "engine/threads/mutex.h"

#include <array>
#include <span>
#include <string_view>

namespace simcoe {
    struct IService;

    using ServiceSpan = std::span<IService*>;

    constexpr auto depends(auto&&... args) {
        return std::array<IService*, sizeof...(args)>{ args... };
    }

    enum ServiceLoadFlags {
        eServiceLoadDefault = 0, // load the service with default settings
        eServiceLoadMainThread = (1 << 0), // load the service on the main thread
    };

    enum ServiceState {
        eServiceInitial = (1 << 0), // service has not been setup yet
        eServiceSetup   = (1 << 1), // service has been setup
        eServiceCreated = (1 << 2), // service has been created
        eServiceFaulted = (1 << 3) // service has been created but failed to initialize
    };

    struct IService {
        SM_NOCOPY(IService)

        IService() = delete;

        IService(std::string_view name, ServiceSpan deps, ServiceLoadFlags flags)
            : name(name)
            , deps(deps)
            , flags(flags)
            , cvMutex(std::format("{}.cv", name))
        { }

        virtual ~IService() = default;

        void create();
        void destroy();

        void waitUntilReady();

        std::string_view getName() const { return name; }
        ServiceSpan getServiceDeps() const { return deps; }
        ServiceLoadFlags getFlags() const { return flags; }

    protected:
        virtual bool createService() = 0;
        virtual void destroyService() = 0;

        ServiceState getState() const { return state; }

    private:
        std::atomic<ServiceState> state = eServiceInitial;

        std::string_view name;
        ServiceSpan deps;
        ServiceLoadFlags flags;

        void waitForDeps();
        void signalReady();

        std::condition_variable cv;
        mt::Mutex cvMutex;
    };

    template<typename T>
    constexpr ServiceLoadFlags getLoadFlags() {
        if constexpr (requires { T::kServiceFlags; }) {
            return T::kServiceFlags;
        } else {
            return eServiceLoadDefault;
        }
    }

    template<typename T>
    struct IStaticService : public IService {
        using IService::IService;

        SM_NOMOVE(IStaticService)

        IStaticService()
            : IService(T::kServiceName, T::kServiceDeps, getLoadFlags<T>())
        { }

        static T *get() {
            static T instance;
            return &instance;
        }

        static IService *service() {
            return static_cast<IService*>(get());
        }

        static ServiceState getState() {
            return get()->IService::getState();
        }

        static std::string_view getFailureReason() {
            return get()->failure;
        }

    protected:
        void setFailureReason(std::string reason) {
            failure = std::move(reason);
        }

    private:
        std::string failure;
    };

    struct ServiceRuntime {
        ServiceRuntime(std::span<IService*> services);
        ~ServiceRuntime();

    private:
        std::span<IService*> services;
    };
}
