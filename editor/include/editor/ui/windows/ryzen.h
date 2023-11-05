#pragma once

#include "editor/ui/service.h"
#include "editor/ui/components/buffer.h"

#include "vendor/amd/ryzen.h"

namespace editor::ui {
    struct CoreInfoHistory {
        CoreInfoHistory();

        void addFrequency(float time, float f);
        void addResidency(float time, float r);

        float lastFrequency = 0.f;
        float lastResidency = 0.f;

        // we only need a minute of history
        ScrollingBuffer frequency = { 60 };
        ScrollingBuffer residency = { 60 };
    };

    struct RyzenMonitorDebug final : ServiceDebug {
        RyzenMonitorDebug();

        void draw() override;
        void drawWindow() override;

        void updateCoreInfo();

    private:
        void drawBiosInfo();

        enum HoverMode : int { eHoverNothing, eHoverCurrent, eHoverHistory };
        enum DisplayMode : int { eDisplayCurrent, eDisplayHistory };

        static constexpr auto kHoverNames = std::to_array({ "Nothing", "Current Values", "History" });
        static constexpr auto kDisplayNames = std::to_array({ "Current Value", "History" });

        HoverMode hoverMode = eHoverHistory;
        DisplayMode displayMode = eDisplayCurrent;

        bool bShowFrequency = true;
        bool bShowResidency = true;

        void drawCoreHistory(size_t i, float width, float heightRatio, bool bHover);

        void drawCoreHover(size_t i);

        void drawCoreInfoCurrentData();
        void drawCoreInfoHistory();

        void drawCpuInfo();
        void drawPackageInfo();
        void drawSocInfo();
        void drawCoreInfo();

        static ImVec4 getUsageColour(float f);

        amd::PackageData packageData = {};
        amd::SocData socData = {};
        std::vector<CoreInfoHistory> coreData;

        std::mutex monitorLock;
        bool bInfoDirty = true;
        size_t updates = 0;

        Clock clock;
        float lastUpdate = 0.f;
    };
}
