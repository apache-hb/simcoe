#include "vendor/ryzenmonitor/service.h"

std::string_view amd::toString(OcMode mode) {
    switch (mode) {
    case OcMode::eModeManual: return "manual";
    case OcMode::eModePbo: return "pbo";
    case OcMode::eModeAuto: return "auto";
    case OcMode::eModeEco: return "eco";
    case OcMode::eModeDefault: return "default";
    default: return "unknown";
    }
}
