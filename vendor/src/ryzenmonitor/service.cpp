#include "vendor/ryzenmonitor/service.h"

#include "engine/log/service.h"
#include "engine/service/platform.h"

#include "engine/core/unique.h"
#include "engine/core/strings.h"

#include "ryzenmonitor/IPlatform.h"
#include "ryzenmonitor/IDeviceManager.h"

#include <filesystem>
#include <unordered_map>

#include <intrin.h>
#include <tchar.h>
#include <shlobj.h>
#include <versionhelpers.h>
#include <lm.h>

#if SM_SERVICE_RYZENMONITOR != 1
#   error "Compiling RyzenMonitorSerivce without RyzenMonitor enabled"
#endif

using namespace simcoe;
using namespace amd;

namespace fs = std::filesystem;

namespace {
    constexpr auto *pServiceName = _T("AMDRyzenMasterDriverV22");
    constexpr auto *kDriverPathEnv = L"AMDRMMONITORSDKPATH";

    // amd products never really feel finished do they
    // this is the mangled version of `GetPlatform`, they forgot to put extern "C" on it
    constexpr auto *kGetPlatformSymbol = "?GetPlatform@@YAAEAVIPlatform@@XZ";

    constexpr std::string_view kAuthenticAmd = "AuthenticAMD";

    constexpr int kWorkInfoLevel = 100;
    constexpr DWORD kDefaultAccess = SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CREATE_SERVICE;
    constexpr DWORD kSearchFlags = LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32;

    using DeleteScHandle = decltype([](SC_HANDLE handle) { CloseServiceHandle(handle); });

    using ServiceHandle = core::UniqueHandle<SC_HANDLE, DeleteScHandle, nullptr>;

    using InfoSink = std::unordered_map<std::string_view, std::string>;
    using CpuVendor = std::array<char, 20>;

    CpuVendor getCpuVendor() {
        CpuVendor vendor = {};

        int cpuInfo[4];

        // TODO: extract cpuid to a util function
#if defined(_MSC_VER) && !defined(__clang__)
        __cpuid(cpuInfo, 0);
#else
        __cpuid(0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#endif
        // the vendor string is stored in a weird pattern
        memcpy(vendor.data(), &cpuInfo[1], sizeof(uint32_t));
        memcpy(vendor.data() + sizeof(uint32_t), &cpuInfo[3], sizeof(uint32_t));
        memcpy(vendor.data() + sizeof(uint32_t) * 2, &cpuInfo[2], sizeof(uint32_t));

        return vendor;
    }

    enum PackageType {
        eFP5 = 0,
        eAM5 = 0,
        eFP7 = 1,
        eFL1 = 1,
        eFP8 = 1,
        eAM4 = 2,
        eFP7r2 = 2,
        eAM5_B0 = 3,
        eSP3 = 4,
        eFP7_B0 = 4,
        eFP7R2_B0 = 5,
        eSP3r2 = 7,
        eUnknown = 0xF
    };

    constexpr auto kFp5Support = std::to_array<unsigned>({
        0x00810F80, 0x00810F81, 0x00860F00, 0x00860F01,
        0x00A50F00, 0x00A50F01, 0x00860F81, 0x00A60F00,
        0x00A60F01, 0x00A60F10, 0x00A60F11, 0x00A60F12
    });

    constexpr auto kAm4Support = std::to_array<unsigned>({
        0x00800F00, 0x00800F10, 0x00800F11, 0x00800F12,
        0x00810F10, 0x00810F11, 0x00800F82, 0x00800F83,
        0x00870F00, 0x00870F10, 0x00810F80, 0x00810F81,
        0x00860F00, 0x00860F01, 0x00A20F00, 0x00A20F10,
        0x00A20F12, 0x00A50F00, 0x00A50F01, 0x00A40F00,
        0x00A40F40, 0x00A40F41, 0x00A70F00, 0x00A70F40,
        0x00A70F41, 0x00A70F42, 0x00A70F80,
    });

    constexpr auto kSp3r2Support = std::to_array<unsigned>({
        0x00800F10, 0x00800F11, 0x00800F12, 0x00800F82,
        0x00800F83, 0x00830F00, 0x00830F10,
    });

    constexpr auto kSp3Support = std::to_array<unsigned>({
        0x00A40F00, 0x00A40F40, 0x00A40F41, 0x00A60F11,
        0x00A60F12, 0x00A00F80, 0x00A00F82, 0x00A70F00,
        0x00A70F40, 0x00A70F41, 0x00A70F42, 0x00A70F80,
    });

    constexpr bool checkSupportedIds(unsigned id, std::span<const unsigned> ids) {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    }

    bool isProcessorSupported(InfoSink& sink) {
        int cpuInfo[4];

#if defined(_MSC_VER) && !defined(__clang__)
        __cpuid(cpuInfo, 0x80000001);
#else
        __cpuid(0x80000001, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#endif

        unsigned id = cpuInfo[0];
        PackageType type = PackageType(cpuInfo[1] >> 28);
        sink["cpuid"] = fmt::format("{:#08x}", id);
        sink["package"] = std::to_string(int(type));

        switch (type) {
        case eFP5:
            return checkSupportedIds(id, kFp5Support);

        case eAM4: case eFP7R2_B0:
            return checkSupportedIds(id, kAm4Support);

        case eSP3r2:
            return checkSupportedIds(id, kSp3r2Support);

        case eFP7: case eSP3:
            return checkSupportedIds(id, kSp3Support);

        default:
            return false;
        }
    }


    bool isAuthenticAmd(InfoSink& sink) {
        const auto vendor = getCpuVendor();
        sink["vendor"] = vendor.data();

        return kAuthenticAmd == vendor.data();
    }

    bool isWindowsSupported(InfoSink& sink) {
        if (IsWindowsServer()) {
            LOG_ERROR("Windows Server is not supported");
            return false;
        }

        if (IsWindows10OrGreater()) {
            return true;
        }

        DWORD major = 0;
        DWORD minor = 0;
        LPBYTE pInfoBuffer = nullptr;

        if (NET_API_STATUS status = NetWkstaGetInfo(nullptr, kWorkInfoLevel, &pInfoBuffer); status == NERR_Success) {
            WKSTA_INFO_100 *pInfo = reinterpret_cast<WKSTA_INFO_100*>(pInfoBuffer);
            major = pInfo->wki100_ver_major;
            minor = pInfo->wki100_ver_minor;
            NetApiBufferFree(pInfoBuffer);
        }

        sink["windows"] = fmt::format("{}.{}", major, minor);

        if (major >= 10) {
            return true;
        }

        LOG_ERROR("Windows version {}.{} is unsupported", major, minor);
        return false;
    }

    fs::path getDriverPath() {
        size_t size = 0;
        _wgetenv_s(&size, nullptr, 0, kDriverPathEnv);
        if (size == 0) {
            LOG_ERROR("{} env var not set", util::narrow(kDriverPathEnv));
            return {};
        }

        std::wstring path(size + 1, wchar_t(0));
        errno_t err = _wgetenv_s(&size, path.data(), size, kDriverPathEnv);
        if (err != 0) {
            LOG_ERROR("Failed to get driver path (err={})", err);
            return {};
        }

        return fs::path(path).parent_path();
    }

    std::string joinFields(const InfoSink& sink) {
        std::vector<std::string> fields;
        for (const auto& [key, field] : sink) {
            fields.emplace_back(fmt::format("{} = {}", key, field));
        }

        return util::join(fields, "\n");
    }
}

bool RyzenMonitorSerivce::createService() {
    InfoSink fields;

    auto fail = [&]<typename... A>(std::string_view fmt, A&&... args) {
        auto reason = fmt::vformat(fmt, fmt::make_format_args(args...));
        LOG_ERROR("RyzenMonitorSerivce setup failed: {}\n{}", reason, joinFields(fields));
        setFailureReason(reason);
        return false;
    };

    fs::path driverDir = getDriverPath();
    if (driverDir.empty()) {
        return fail("Driver path not set");
    }

    fields["driverdir"] = driverDir.string();

    if (!isAuthenticAmd(fields)) {
        return fail("Processor is not AMD");
    }

    if (!isProcessorSupported(fields)) {
        return fail("Unsupported processor");
    }

    if (!isWindowsSupported(fields)) {
        return fail("Unsupported OS");
    }

    if (!IsUserAnAdmin()) {
        return fail("User is not admin");
    }

    // try and open the service manager
    ServiceHandle hManager = OpenSCManager(nullptr, nullptr, kDefaultAccess);
    if (hManager == nullptr) {
        return fail("Failed to open service manager (err={})", debug::getErrorName());
    }

    // does the service exist?
    ServiceHandle hService = OpenService(hManager.get(), pServiceName, kDefaultAccess);
    if (DWORD err = GetLastError(); err == ERROR_SERVICE_DOES_NOT_EXIST) {
        return fail("Driver is not installed");
    }

    SERVICE_STATUS status = {};
    if (!QueryServiceStatus(hService.get(), &status)) {
        return fail("Failed to query service status (err={})", debug::getErrorName());
    }

    if (status.dwCurrentState != SERVICE_RUNNING) {
        return fail("Driver is not running (state={:#08x})", status.dwCurrentState);
    }

    // TODO: try and install the driver if it's not installed

    fs::path driverBinDir = driverDir / "bin";

    fields["driverbin"] = driverBinDir.string();
    if (!AddDllDirectory(driverBinDir.c_str())) {
        return fail("Failed to add driver bin directory to dll search path (err={})", debug::getErrorName());
    }

    hPlatformModule = LoadLibraryEx("Platform", nullptr, kSearchFlags);
    if (hPlatformModule == nullptr) {
        return fail("Failed to load `Platform.dll` (err={})", debug::getErrorName());
    }

    auto *pGetPlatform = (decltype(&GetPlatform))GetProcAddress(hPlatformModule, kGetPlatformSymbol);
    if (pGetPlatform == nullptr) {
        return fail("Failed to get `GetPlatform` function (err={})", debug::getErrorName());
    }

    // TODO: is this UB?
    pPlatform = &pGetPlatform();
    if (!pPlatform->Init()) {
        return fail("Failed to initialize platform");
    }

    pManager = &pPlatform->GetIDeviceManager();

    pBiosInfo = nullptr;
    pCpuInfo = nullptr;

    setupBiosDevices();
    setupCpuDevices();

    LOG_INFO("RyzenMonitorSerivce setup complete\n{}", joinFields(fields));

    return true;
}

void RyzenMonitorSerivce::destroyService() {
    delete pCpuInfo;
    delete pBiosInfo;

    if (pPlatform != nullptr)
        pPlatform->UnInit();

    if (hPlatformModule != nullptr)
        FreeLibrary(hPlatformModule);
}

// public api

const BiosInfo *RyzenMonitorSerivce::getBiosInfo() {
    return get()->pBiosInfo;
}

const CpuInfo *RyzenMonitorSerivce::getCpuInfo() {
    return get()->pCpuInfo;
}

bool RyzenMonitorSerivce::updateCpuInfo() {
    return get()->pCpuInfo->refresh();
}

// internals

void RyzenMonitorSerivce::setupBiosDevices() {
    if (IBIOSEx *pBios = reinterpret_cast<IBIOSEx*>(pManager->GetDevice(dtBIOS, 0)); pBios == nullptr) {
        LOG_ERROR("Failed to get BIOS device, driver is probably busted");
    } else {
        pBiosInfo = new BiosInfo(pBios);
    }
}

void RyzenMonitorSerivce::setupCpuDevices() {
    if (ICPUEx *pCpu = reinterpret_cast<ICPUEx*>(pManager->GetDevice(dtCPU, 0)); pCpu == nullptr) {
        LOG_ERROR("Failed to get CPU device, driver is probably busted");
    } else {
        pCpuInfo = new CpuInfo(pCpu);
    }
}
