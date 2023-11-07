#pragma once

#include "engine/debug/service.h"

#include "XSystem.h"
#include "XGameRuntimeFeature.h"

namespace microsoft {
    struct GdkFeature {
        std::string_view name;
        bool bEnabled;
    };

    using GdkFeatureSet = std::array<GdkFeature, 22>; // update this if XGameRuntimeFeature gets more fields

    struct GdkService final : simcoe::IStaticService<GdkService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "gdk";
        static constexpr std::array kServiceDeps = { simcoe::DebugService::kServiceName };

        // IService
        bool createService() override;
        void destroyService() override;

        // gdk info
        static const XSystemAnalyticsInfo& getAnalyticsInfo();
        static const GdkFeatureSet& getFeatures();
        static std::string_view getConsoleId();

    private:
        // initialized when gdk is enabled
        GdkFeatureSet features = {};
        XSystemAnalyticsInfo analyticsInfo = {};
        std::array<char, XSystemConsoleIdBytes + 1> consoleId = {};
    };
}
