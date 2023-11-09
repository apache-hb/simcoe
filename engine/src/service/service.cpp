#include "engine/service/service.h"

#include "engine/core/error.h"
#include "engine/log/service.h"

#include "engine/core/panic.h"

#include <unordered_set>

using namespace simcoe;

// service base

void IService::create() try {
    if (state == eServiceCreated) {
        LOG_INFO("service {} already created, skipping setup", getName());
        return;
    }

    if (createService()) {
        state = eServiceCreated;
    } else {
        LOG_ERROR("failed to load {} service", getName());
        state = eServiceFaulted;
    }
} catch (const core::Error& err) {
    LOG_ERROR("failed to load {} service: {}", getName(), err.what());
    for (const auto& frame : err.getStacktrace()) {
        LOG_ERROR("  {}", frame.symbol);
    }
    state = eServiceFaulted;

    if (!err.recoverable()) { throw; }
}

void IService::destroy() {
    if (state != eServiceCreated) {
        LOG_INFO("service {} not created, skipping teardown", getName());
        return;
    }

    LOG_INFO("unloading {} service", getName());
    destroyService();
    state = eServiceInitial;
}

// service runtime

ServiceRuntime::ServiceRuntime(std::span<IService*> services)
    : services(services)
{
    LOG_INFO("loading {} services", services.size());
    std::unordered_set<std::string_view> loaded;

    for (IService *pService : services) {
        auto name = pService->getName();
        SM_ASSERTF(!loaded.contains(name), "{} service already loaded", name);

        for (std::string_view dep : pService->getDeps()) {
            SM_ASSERTF(loaded.contains(dep), "{} depends on {}, but it's not loaded", name, dep);
        }

        pService->create();
        loaded.emplace(name);
        LOG_INFO("loaded {} service", name);
    }
}

ServiceRuntime::~ServiceRuntime() {
    for (size_t i = services.size(); i-- > 0;) {
        services[i]->destroy();
    }
}
