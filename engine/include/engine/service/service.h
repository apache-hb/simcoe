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

        IService() = default;
        virtual ~IService() = default;

        virtual std::string_view getName() const = 0;
        virtual NameSpan getDeps() const = 0;

        void create();
        void destroy();

    protected:
        virtual bool createService() = 0;
        virtual void destroyService() = 0;

        ServiceState getState() const { return state; }

    private:
        ServiceState state = eServiceInitial;
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

        static ServiceState getState() {
            return get()->IService::getState();
        }
    };

    struct ServiceRuntime {
        ServiceRuntime(std::span<IService*> services);
        ~ServiceRuntime();

    private:
        std::span<IService*> services;
    };
}
