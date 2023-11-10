#include "engine/service/service.h"

#include "engine/core/error.h"
#include "engine/core/panic.h"

#include "engine/log/service.h"

#include "engine/profile/profile.h"

#include <future>
#include <unordered_set>
#include <execution>

using namespace simcoe;

// service base

void IService::waitUntilReady() {
    std::unique_lock lock(mutex.getInner());
    cv.wait(lock, [this] { return state != eServiceInitial; });
}

void IService::waitForDeps() {
    for (IService *pService : getServiceDeps()) {
        pService->waitUntilReady();
    }
}

void IService::signalReady() {
    std::unique_lock lock(mutex);
    cv.notify_all();
}

void IService::create() try {
    const auto& serviceName = getName();
    if (state == eServiceCreated) {
        LOG_INFO("service {} already created, skipping setup", serviceName);
        return;
    }

    waitForDeps();
    LOG_INFO("loading {} service", serviceName);

    ZoneNamed(perfServiceCreateZone, true);
    perfServiceCreateZone.Name(serviceName.data(), serviceName.size());
    Clock clock;

    if (createService()) {
        LOG_INFO("loaded {} service in {}ms", serviceName, clock.ms());
        state = eServiceCreated;
    } else {
        LOG_ERROR("failed to load {} service", serviceName);
        state = eServiceFaulted;
    }

    signalReady();
} catch (const core::Error& err) {
    state = eServiceFaulted;
    signalReady();

    LOG_ERROR("failed to load {} service: {}", getName(), err.what());
    for (const auto& frame : err.getStacktrace()) {
        LOG_ERROR("  {}", frame.symbol);
    }

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

ServiceRuntime::ServiceRuntime(ServiceSpan services)
    : services(services)
{
    Clock clock;
    LOG_INFO("loading {} services", services.size());

    std::vector<std::future<void>> workThreadServices;
    std::vector<IService*> mainThreadServices;

    for (IService *pService : services) {
        if (pService->getFlags() & eServiceLoadMainThread) {
            mainThreadServices.push_back(pService);
        } else {
            workThreadServices.push_back(std::async(std::launch::async, [pService] {
                pService->create();
            }));
        }
    }

    for (IService *pService : mainThreadServices) {
        pService->create();
    }

    for (auto& future : workThreadServices) {
        future.wait();
    }

    LOG_INFO("loaded {} services (took {}ms)", services.size(), clock.ms());
}

ServiceRuntime::~ServiceRuntime() {
    for (size_t i = services.size(); i-- > 0;) {
        services[i]->destroy();
    }
}
