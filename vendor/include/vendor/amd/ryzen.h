#pragma once

#include "engine/service/service.h"

#include "engine/core/units.h"
#include "engine/core/win32.h"

#include "ryzen/IPlatform.h"

#include <array>
#include <vector>

class IBIOSEx;
class ICPUEx;

namespace amd {
    namespace core = simcoe::core;

    struct CoreInfo {
        float frequency = 0.f;
        float residency = 0.f;
    };

    struct SocData {
        float voltage = 0.f;
    };

    enum struct OcMode {
        eModeManual,
        eModePbo,
        eModeAuto,
        eModeEco,
        eModeDefault
    };

    std::string_view toString(OcMode mode);

    template<typename T>
    struct MonitorObject {
        MonitorObject(T *pDevice) : pDevice(pDevice) { }

    protected:
        T *pDevice;
    };

    struct BiosInfo : MonitorObject<IBIOSEx> {
        BiosInfo(IBIOSEx *pDevice);

        std::string_view getVersion() const noexcept { return version; }
        std::string_view getVendor() const noexcept { return vendor; }
        core::Date getDate() const noexcept { return date; }

    private:
        std::string version;
        std::string vendor;
        core::Date date;
    };

    struct PackageInfo {
        float peakSpeed = 0.f;
        float baseSpeed = 0.f;
    };

    struct CpuInfo : MonitorObject<ICPUEx> {
        CpuInfo(ICPUEx *pDevice);

        std::string_view getName() const noexcept { return name; }
        std::string_view getDescription() const noexcept { return description; }
        std::string_view getVendor() const noexcept { return vendor; }
        std::string_view getRole() const noexcept { return role; }
        std::string_view getClassName() const noexcept { return className; }
        std::string_view getPackage() const noexcept { return package; }

        unsigned getCoreCount() const noexcept { return coreCount; }
        unsigned getCorePark() const noexcept { return corePark; }

        OcMode getMode() const noexcept { return mode; }

        std::span<CoreInfo> getCoreData() const noexcept { return { cores.get(), coreCount }; }
        SocData getSocData() const noexcept { return socData; }

        /**
         * @brief fetch new cpu info
         *
         * @return true if success
         * @return false if failed
         */
        bool refresh();

    private:
        std::string name;
        std::string description;
        std::string vendor;
        std::string role;
        std::string className;
        std::string package;

        unsigned coreCount = 0;
        unsigned corePark = 0;

        float peakSpeed = 0.f;

        OcMode mode = OcMode::eModeDefault;
        SocData socData;
        std::unique_ptr<CoreInfo[]> cores;
    };

    struct RyzenMonitorSerivce : simcoe::IStaticService<RyzenMonitorSerivce> {
        // IStaticService
        static constexpr std::string_view kServiceName = "ryzenmonitor";
        static constexpr std::array<std::string_view, 0> kServiceDeps = { };

        // IService
        bool createService() override;
        void destroyService() override;

        // failure reason
        static std::string_view getFailureReason();

        // monitor info
        static const BiosInfo *getBiosInfo();
        static const CpuInfo *getCpuInfo();

        // update info
        static void updateCpuInfo();

    private:
        static bool isAuthenticAmd();
        static bool isWindowsSupported();

        void setupBiosDevices();
        void setupCpuDevices();

        // error info
        std::string error = "";

        // dll handles
        HMODULE hPlatformModule = nullptr;
        //HMODULE hDeviceModule = nullptr;

        // interfaces
        IPlatform *pPlatform = nullptr;
        IDeviceManager *pManager = nullptr;

        // device info
        BiosInfo *pBiosInfo = nullptr;
        CpuInfo *pCpuInfo = nullptr;
    };
}
