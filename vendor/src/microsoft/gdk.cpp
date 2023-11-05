#include "vendor/microsoft/gdk.h"

#include "engine/log/service.h"

#include "XGameRuntime.h"
#include "XGameRuntimeFeature.h"
#include "XGameErr.h"

using namespace microsoft;
using namespace simcoe;

#define HR_CHECK(expr) \
    do { \
        if (HRESULT hr = (expr); FAILED(hr)) { \
            auto msg = std::format("gdk-error: {} ({})", #expr, gdkErrorString(hr)); \
            LOG_ERROR(msg); \
            throw std::runtime_error(msg); \
        } \
    } while (false)

#define E_GAME_MISSING_GAME_CONFIG ((HRESULT)0x87E5001FL)

namespace {
    std::string gdkErrorString(HRESULT hr) {
        switch (hr) {
        case E_GAME_MISSING_GAME_CONFIG: return "gdk:config-missing";
        case E_GAMERUNTIME_NOT_INITIALIZED: return "gdk:not-initialized";
        case E_GAMERUNTIME_DLL_NOT_FOUND: return "gdk:dll-not-found";
        case E_GAMERUNTIME_VERSION_MISMATCH: return "gdk:version-mismatch";
        case E_GAMERUNTIME_WINDOW_NOT_FOREGROUND: return "gdk:window-not-foreground";
        case E_GAMERUNTIME_SUSPENDED: return "gdk:suspended";

        default: return debug::getResultName(hr);
        }
    }
}

#define CHECK_FEATURE(KEY) \
    do { \
        features[size_t(XGameRuntimeFeature::KEY)] = { .name = #KEY, .bEnabled = XGameRuntimeIsFeatureAvailable(XGameRuntimeFeature::KEY) }; \
    } while (false)

bool GdkService::createService() {
    if (HRESULT hr = XGameRuntimeInitialize(); FAILED(hr)) {
        auto err = gdkErrorString(hr);
        LOG_ERROR("XGameRuntimeInitialize() = {}", err);
        return false;
    }

    analyticsInfo = XSystemGetAnalyticsInfo();

    size_t size = consoleId.size();
    HR_CHECK(XSystemGetConsoleId(consoleId.size(), consoleId.data(), &size));
    consoleId[size] = '\0';

    CHECK_FEATURE(XAccessibility);
    CHECK_FEATURE(XAppCapture);
    CHECK_FEATURE(XAsync);
    CHECK_FEATURE(XAsyncProvider);
    CHECK_FEATURE(XDisplay);
    CHECK_FEATURE(XGame);
    CHECK_FEATURE(XGameInvite);
    CHECK_FEATURE(XGameSave);
    CHECK_FEATURE(XGameUI);
    CHECK_FEATURE(XLauncher);
    CHECK_FEATURE(XNetworking);
    CHECK_FEATURE(XPackage);
    CHECK_FEATURE(XPersistentLocalStorage);
    CHECK_FEATURE(XSpeechSynthesizer);
    CHECK_FEATURE(XStore);
    CHECK_FEATURE(XSystem);
    CHECK_FEATURE(XTaskQueue);
    CHECK_FEATURE(XThread);
    CHECK_FEATURE(XUser);
    CHECK_FEATURE(XError);
    CHECK_FEATURE(XGameEvent);
    CHECK_FEATURE(XGameStreaming);

    return true;
}

void GdkService::destroyService() {
    XGameRuntimeUninitialize();
}

// gdk info
const XSystemAnalyticsInfo& GdkService::getAnalyticsInfo() {
    return get()->analyticsInfo;
}

const GdkFeatureSet& GdkService::getFeatures() {
    return get()->features;
}

std::string_view GdkService::getConsoleId() {
    return get()->consoleId.data();
}
