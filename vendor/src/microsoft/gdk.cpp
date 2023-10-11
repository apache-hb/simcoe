#include "vendor/microsoft/gdk.h"

#include "engine/os/system.h"

#include "XGameRuntime.h"
#include "XGameRuntimeFeature.h"
#include "XGameErr.h"

#include <array>

using namespace simcoe;

namespace gdk = microsoft::gdk;

#define HR_CHECK(expr) \
    do { \
        if (HRESULT hr = (expr); FAILED(hr)) { \
            simcoe::logError("failure: {} ({})", #expr, simcoe::getErrorName(hr)); \
            throw std::runtime_error(#expr); \
        } \
    } while (false)

#define E_GAME_MISSING_GAME_CONFIG ((HRESULT)0x87E5001FL)

namespace {
    bool gEnabled = false;
    gdk::FeatureArray gFeatures = {};

    XSystemAnalyticsInfo gInfo = {};
    std::array<char, XSystemConsoleIdBytes> gConsoleId = {};

    std::string gdkErrorString(HRESULT hr) {
        switch (hr) {
        case E_GAME_MISSING_GAME_CONFIG: return "gdk:config-missing";
        case E_GAMERUNTIME_NOT_INITIALIZED: return "gdk:not-initialized";
        case E_GAMERUNTIME_DLL_NOT_FOUND: return "gdk:dll-not-found";
        case E_GAMERUNTIME_VERSION_MISMATCH: return "gdk:version-mismatch";
        case E_GAMERUNTIME_WINDOW_NOT_FOREGROUND: return "gdk:window-not-foreground";
        case E_GAMERUNTIME_SUSPENDED: return "gdk:suspended";

        default: return simcoe::getErrorName(hr);
        }
    }
}

#define CHECK_FEATURE(key) \
    do { \
        gFeatures[size_t(XGameRuntimeFeature::key)] = { .name = #key, .enabled = XGameRuntimeIsFeatureAvailable(XGameRuntimeFeature::key) }; \
    } while (false)

std::string gdk::init() {
    if (HRESULT hr = XGameRuntimeInitialize(); FAILED(hr)) {
        auto err = gdkErrorString(hr);
        simcoe::logError("XGameRuntimeInitialize() = {}", err);
        return err;
    }

    gInfo = XSystemGetAnalyticsInfo();

    size_t size = gConsoleId.size();
    HR_CHECK(XSystemGetConsoleId(gConsoleId.size(), gConsoleId.data(), &size));
    gConsoleId[size - 1] = '\0';

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

    gEnabled = true;
    return "";
}

void gdk::deinit() {
    XGameRuntimeUninitialize();
}

bool gdk::enabled() {
    return gEnabled;
}

XSystemAnalyticsInfo& gdk::getAnalyticsInfo() {
    return gInfo;
}

const gdk::FeatureArray& gdk::getFeatures() {
    return gFeatures;
}

std::string_view gdk::getConsoleId() {
    return gConsoleId.data();
}
