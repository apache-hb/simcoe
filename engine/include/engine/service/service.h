#pragma once

#include "engine/core/macros.h"
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
        SM_NOCOPY(IService)

        IService(std::string_view name)
            : name(name)
        { }

        IService() = default;
        virtual ~IService() = default;

        void create();
        void destroy();
        std::string_view getName() const { return name; }

        virtual NameSpan getDeps() const = 0;

    protected:
        virtual bool createService() = 0;
        virtual void destroyService() = 0;

        ServiceState getState() const { return state; }
    private:
        ServiceState state = eServiceInitial;
        std::string_view name;
    };

    template<typename T>
    struct IStaticService : IService {
        using IService::IService;
        SM_NOMOVE(IStaticService)

        IStaticService() : IService(T::kServiceName) { }

        NameSpan getDeps() const override { return T::kServiceDeps; }

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
