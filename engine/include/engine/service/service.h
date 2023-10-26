#pragma once

#include <span>
#include <string_view>
#include <atomic>

#include "engine/engine.h"

namespace simcoe {
    using NameSpan = std::span<const std::string_view>;

    struct IService {
        virtual ~IService() = default;

        virtual std::string_view getName() const = 0;
        virtual NameSpan deps() const = 0; // TODO: implement deps

        void create() {
            ASSERTF(!bCreated, "service {} already created", getName());
            createService();
            bCreated = true;
        }

        void destroy() {
            ASSERTF(bCreated, "service {} not created", getName());
            destroyService();
            bCreated = false;
        }

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
        NameSpan deps() const override { return T::kServiceDeps; }

        static T *get() {
            static T instance;
            return &instance;
        }

        static T *use(const char *fn) {
            ASSERTF(isCreated(), "service {} not created, cannot call {}", T::kServiceName, fn);
            return get();
        }

        static bool isCreated() { return get()->isCreated(); }
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
