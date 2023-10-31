#include "engine/depot/service.h"

using namespace simcoe;

bool DepotService::createService() {
    return true;
}

void DepotService::destroyService() {

}

std::string_view DepotService::getFailureReason() {
    return get()->failureReason;
}
