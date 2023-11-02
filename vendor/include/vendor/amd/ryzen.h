#pragma once

#include "engine/service/service.h"

#include "engine/core/win32.h"
#include "engine/core/units.h"
#include "engine/core/array.h"

#include "ryzen/IPlatform.h"

#include <array>
#include <vector>

class IBIOSEx;
class ICPUEx;

namespace amd {
    namespace core = simcoe::core;

    enum struct OcMode {
        eModeManual,
        eModePbo,
        eModeAuto,
        eModeEco,
        eModeDefault
    };

    struct PackageData {
        OcMode mode = OcMode::eModeDefault;
        float peakSpeed = 0.f;
        float temperature = 0.f;

        float chctCurrentLimit = 0.f; // celcius

        float avgCoreVoltage = 0.f; // volts
        float peakCoreVoltage = 0.f; // volts

        float maxClock = 0.f; // mhz
        float fabricClock = 0.f; // mhz

        float pptCurrentLimit = 0.f; // watts
        float pptCurrentValue = 0.f; // watts

        float edcCurrentLimit = 0.f; // amps
        float edcCurrentValue = 0.f; // amps

        float tdcCurrentLimit = 0.f; // amps
        float tdcCurrentValue = 0.f; // amps
    };

    struct MemoryData {
        uint16_t vddioVoltage = UINT16_MAX; // millivolts
        uint16_t memClock = UINT16_MAX; // mhz

        uint8_t ctrlTcl = 0; // cas latency
        uint8_t ctrlTrcdrd = 0; // read row address to column address delay
        uint8_t ctrlTras = 0; // row active time
        uint8_t ctrlTrp = 0; // row cycle time
    };

    struct CoreInfo {
        float frequency = 0.f;
        float residency = 0.f;
    };

    struct SocData {
        float voltage = 0.f;

        float edcCurrentLimit = 0.f; // amps
        float edcCurrentValue = 0.f; // amps

        float tdcCurrentLimit = 0.f; // amps
        float tdcCurrentValue = 0.f; // amps

        float vddcrVddCurrent = 0.f; // watts
        float vddcrSocCurrent = 0.f; // watts
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

        MemoryData getMemoryData() const noexcept { return memoryInfo; }

    private:
        std::string version;
        std::string vendor;
        core::Date date;

        MemoryData memoryInfo;
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

        std::span<CoreInfo> getCoreData() const noexcept { return { cores.get(), coreCount }; }

        PackageData getPackageData() const noexcept { return packageInfo; }
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

        PackageData packageInfo;

        SocData socData;
        core::UniquePtr<CoreInfo[]> cores;
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
        static bool updateCpuInfo();

    private:
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
