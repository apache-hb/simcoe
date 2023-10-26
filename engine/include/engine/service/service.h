#pragma once

#include "engine/core/panic.h"

#include <span>
#include <string_view>
#include <atomic>

namespace simcoe {
    using NameSpan = std::span<const std::string_view>;

    struct IService {
        virtual ~IService() = default;

        virtual std::string_view getName() const = 0;
        virtual NameSpan getDeps() const = 0;

        void create();
        void destroy();

    protected:
        virtual void createService() = 0;
        virtual void destroyService() = 0;

        bool isCreated() const { return bCreated; }

    private:
        std::atomic<bool> bCreated = false;
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

        static T *use(const char *fn) {
            ASSERTF(isCreated(), "service {} not created, cannot call {}", T::kServiceName, fn);
            return get();
        }

        static bool isCreated() { return get()->IService::isCreated(); }
    };

    struct ServiceRuntime {
        ServiceRuntime(std::span<IService*> services);
        ~ServiceRuntime();

    private:
        std::span<IService*> services;
    };
}

#define USE_SERVICE(fn) \
    use(#fn)->fn
