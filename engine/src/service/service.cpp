#include "engine/service/service.h"

#include "engine/core/panic.h"

#include <unordered_set>

using namespace simcoe;

// service base

void IService::create() {
    ASSERTF(!bCreated, "service {} already created", getName());
    createService();
    bCreated = true;
}

void IService::destroy() {
    ASSERTF(bCreated, "service {} not created", getName());
    destroyService();
    bCreated = false;
}

// service runtime

ServiceRuntime::ServiceRuntime(std::span<IService*> services)
    : services(services)
{
    std::unordered_set<std::string_view> loaded;

    for (IService *pService : services) {
        auto name = pService->getName();
        ASSERTF(!loaded.contains(name), "service {} already loaded", name);

        for (std::string_view dep : pService->getDeps()) {
            ASSERTF(loaded.contains(dep), "service {} depends on {}, but it's not loaded", name, dep);
        }

        pService->create();
        loaded.emplace(name);
    }
}

ServiceRuntime::~ServiceRuntime() {
    for (IService *pService : services) {
        pService->destroy();
    }
}
