#pragma once

#include "editor/debug/debug.h"

#include "vendor/amd/ryzen.h"
#include "vendor/microsoft/gdk.h"

#include "engine/service/platform.h"

namespace editor::debug {
    // utility structure for realtime plot
    struct ScrollingBuffer {
        int MaxSize;
        int Offset;
        ImVector<ImVec2> Data;
        ScrollingBuffer(int max_size = 2000) {
            MaxSize = max_size;
            Offset  = 0;
            Data.reserve(MaxSize);
        }

        void AddPoint(float x, float y) {
            if (Data.size() < MaxSize)
                Data.push_back(ImVec2(x,y));
            else {
                Data[Offset] = ImVec2(x,y);
                Offset =  (Offset + 1) % MaxSize;
            }
        }

        void Erase() {
            if (Data.size() > 0) {
                Data.shrink(0);
                Offset  = 0;
            }
        }
    };

    struct CoreInfoHistory {
        void addFrequency(float time, float f) {
            lastFrequency = f;
            frequency.AddPoint(time, f);
        }

        void addResidency(float time, float r) {
            lastResidency = r;
            residency.AddPoint(time, r);
        }

        float lastFrequency = 0.f;
        float lastResidency = 0.f;

        ScrollingBuffer frequency;
        ScrollingBuffer residency;
    };

    struct RyzenMonitorDebug final : ServiceDebug {
        RyzenMonitorDebug();

        void draw() override;

        std::jthread getWorkThread();

    private:
        void drawBiosInfo();

        size_t cols = 4;
        enum HoverMode : int { eHoverNothing, eHoverCurrent, eHoverHistory };
        enum DisplayMode : int { eDisplayName, eDisplayCurrent, eDisplayHistory };

        static constexpr auto kHoverNames = std::to_array({ "Nothing", "Current Values", "History" });
        static constexpr auto kDisplayNames = std::to_array({ "Name", "Current Value", "History" });

        HoverMode hoverMode = eHoverHistory;
        DisplayMode displayMode = eDisplayName;

        bool bShowFrequency = true;
        bool bShowResidency = true;

        void drawCoreHistory(size_t i, float width, float heightRatio);

        void drawCore(size_t i);

        void drawCpuInfo();

        void updateCoreInfo();

    private:
        amd::OcMode mode = amd::OcMode::eModeDefault;
        amd::SocData socData = {};
        std::vector<CoreInfoHistory> coreData;

        std::mutex monitorLock;
        std::atomic_bool bInfoDirty = true;

        Clock clock;
        float lastUpdate = 0.f;
    };

    struct GdkDebug final : ServiceDebug {
        GdkDebug();

        void draw() override;
    };
}
