#include "vendor/amd/ryzen.h"

#include "engine/service/logging.h"

#include "engine/util/strings.h"

#include "ryzen/IBIOSEx.h"
#include "ryzen/ICPUEx.h"

using namespace amd;
using namespace simcoe;

namespace {
    core::Date parseDate(const std::string& text) {
        // date is in format "yyyy/mm/dd"
        if (text.size() < 10) {
            return {};
        }

        int year = std::stoi(text.substr(0, 4));
        int month = std::stoi(text.substr(5, 2));
        int day = std::stoi(text.substr(8, 2));
        LOG_INFO("parsed date: {}-{}-{}", year, month, day);
        return core::Date(core::Day(day), core::Month(month), core::Year(year));
    }

    std::string cvtField(const wchar_t *pText) {
        auto str = util::narrow(pText);
        while (!str.empty() && str.back() == ' ') {
            str.pop_back();
        }
        return str;
    }

    std::string errorString(int err) {
        switch (err) {
        case 0: return "failure";
        case 1: return "success";
        case 2: return "invalid value";
        case 3: return "method not implemented by the BIOS";
        case 4: return "cores are already parked";
        case 5: return "unsupported function";
        default: return std::format("unknown error {}", err);
        }
    }
}

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

// bios info

BiosInfo::BiosInfo(IBIOSEx *pDevice)
    : MonitorObject(pDevice)
{
    version = cvtField(pDevice->GetVersion());
    vendor = cvtField(pDevice->GetVendor());
    date = parseDate(cvtField(pDevice->GetDate()));
}

// cpu info

CpuInfo::CpuInfo(ICPUEx *pDevice)
    : MonitorObject(pDevice)
{
    name = cvtField(pDevice->GetName());
    description = cvtField(pDevice->GetDescription());
    vendor = cvtField(pDevice->GetVendor());
    role = cvtField(pDevice->GetRole());
    className = cvtField(pDevice->GetClassName());
    package = cvtField(pDevice->GetPackage());

    if (int ret = pDevice->GetCoreCount(coreCount)) {
        LOG_INFO("failed to get core count: {}", errorString(ret + 1));
    }

    if (int ret = pDevice->GetCorePark(corePark)) {
        LOG_INFO("failed to get core park: {}", errorString(ret + 1));
    }

    cores = std::make_unique<CoreInfo[]>(coreCount);

    refresh();
}

bool CpuInfo::refresh() {
    CPUParameters params;
    if (int ret = pDevice->GetCPUParameters(params)) {
        LOG_INFO("failed to get cpu parameters: {}", errorString(ret + 1));
        return false;
    }

    if (params.eMode.uManual) {
        mode = OcMode::eModeManual;
    } else if (params.eMode.uPBOMode) {
        mode = OcMode::eModePbo;
    } else if (params.eMode.uAutoOverclocking) {
        mode = OcMode::eModeAuto;
    } else if (params.eMode.uEcoMode) {
        mode = OcMode::eModeEco;
    } else {
        mode = OcMode::eModeDefault;
    }

    socData.voltage = params.dSocVoltage;

    const EffectiveFreqData& fd = params.stFreqData;
    for (unsigned i = 0; i < std::min(fd.uLength, coreCount); i++) {
        cores[i].frequency = fd.dFreq[i];
        cores[i].residency = fd.dState[i];
    }

    return true;
}
