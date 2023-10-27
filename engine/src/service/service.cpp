#include "engine/service/service.h"

#include "engine/service/logging.h"

#include "engine/core/panic.h"

#include <unordered_set>

using namespace simcoe;

// service base

void IService::create() {
    ASSERTF(!bCreated, "service {} already created", getName());

    if (createService()) {
        LOG_INFO("loaded {} service", getName());
        bCreated = true;
    } else {
        LOG_ERROR("failed to load {} service", getName());
    }
}

void IService::destroy() {
    ASSERTF(bCreated, "service {} not created", getName());
    LOG_INFO("unloading {} service", getName());
    destroyService();
    bCreated = false;
}

// service runtime

ServiceRuntime::ServiceRuntime(std::span<IService*> services)
    : services(services)
{
    LOG_INFO("loading {} services", services.size());
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
    for (size_t i = services.size(); i-- > 0;) {
        services[i]->destroy();
    }
}
