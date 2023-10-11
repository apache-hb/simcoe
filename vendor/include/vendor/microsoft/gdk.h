#pragma once

#include <unordered_map>
#include <string_view>

#include <windows.h>

#include "XSystem.h"
#include "XGameRuntimeFeature.h"
#include "GameInput.h"

namespace microsoft::gdk {
    struct FeatureInfo {
        std::string_view name;
        bool enabled;
    };

    // update this if more gdk feature sets are added
    using FeatureArray = std::array<FeatureInfo, 22>;

    std::string init();
    void deinit();
    bool enabled();

    XSystemAnalyticsInfo& getAnalyticsInfo();
    const FeatureArray& getFeatures();
    std::string_view getConsoleId();
}
