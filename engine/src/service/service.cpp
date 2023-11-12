#include "engine/service/service.h"

#include "engine/core/error.h"
#include "engine/core/panic.h"

#include "engine/log/service.h"
#include "engine/threads/service.h"

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

    if (createService()) {
        LOG_INFO("loaded {} service", serviceName);
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

// service startup helpers

static void startService(IService *pService) {
    for (IService *pDep : pService->getServiceDeps()) {
        startService(pDep);
    }

    pService->create();
}

static void stopService(IService *pService) {
    pService->destroy();
}

// service runtime

ServiceRuntime::ServiceRuntime(ServiceSpan services)
    : services(services)
{
    LOG_INFO("loading {} services", services.size());

    // TODO: rework this to be a bit more structured
    // 1. load config service
    // 2. load debug service
    // 3. load thread service
    // 4. load platform service
    // 5. load all other requested services

    startService(DebugService::service());
    startService(ConfigService::service());
    startService(ThreadService::service());
    startService(PlatformService::service());

    for (IService *pService : services) {
        ThreadService::enqueueWork(std::string(pService->getName()), [pService] {
            pService->create();
        });
    }

    for (IService *pService : services) {
        pService->waitUntilReady();
    }

    LOG_INFO("loaded {} services", services.size());
}

ServiceRuntime::~ServiceRuntime() {
    for (size_t i = services.size(); i-- > 0;) {
        IService *pService = services[i];
        pService->destroy();
    }

    stopService(PlatformService::service());
    stopService(ThreadService::service());
    stopService(ConfigService::service());
    stopService(DebugService::service());
}
