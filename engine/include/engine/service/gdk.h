#pragma once

#include "engine/service/debug.h"

#include "XSystem.h"
#include "XGameRuntimeFeature.h"

namespace simcoe {
    struct GdkFeature {
        std::string_view name;
        bool bEnabled;
    };

    using GdkFeatureSet = std::array<GdkFeature, 22>; // update this if XGameRuntimeFeature gets more fields

    struct GdkService final : IStaticService<GdkService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "gdk";
        static constexpr std::array<std::string_view, 0> kServiceDeps = { DebugService::kServiceName };

        // IService
        void createService() override;
        void destroyService() override;

        // GdkService
        static bool isEnabled() { return USE_SERVICE(bEnabled); }
        static std::string_view getFailureReason() { return USE_SERVICE(failureReason); }

        static XSystemAnalyticsInfo& getAnalyticsInfo() { return USE_SERVICE(analyticsInfo); }
        static const GdkFeatureSet& getFeatures() { return USE_SERVICE(features); }
        static std::string_view getConsoleId() { return USE_SERVICE(consoleId).data(); }

    private:
        bool bEnabled = false;

        // initialized when gdk is disabled
        std::string failureReason;

        // initialized when gdk is enabled
        GdkFeatureSet features = {};
        XSystemAnalyticsInfo analyticsInfo = {};
        std::array<char, XSystemConsoleIdBytes + 1> consoleId = {};
    };
}
