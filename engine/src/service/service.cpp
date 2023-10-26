#include "engine/service/service.h"

#include "engine/engine.h"

using namespace simcoe;

// service runtime

ServiceRuntime::ServiceRuntime(std::span<IService*> services)
    : services(services)
{
    for (IService *service : services) {
        service->create();
    }
}

ServiceRuntime::~ServiceRuntime() {
    for (IService *service : services) {
        service->destroy();
    }
}
