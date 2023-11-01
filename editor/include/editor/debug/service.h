#pragma once

#include "editor/debug/debug.h"

#include "vendor/amd/ryzen.h"
#include "vendor/microsoft/gdk.h"

#include "engine/service/platform.h"
#include "engine/threads/service.h"

namespace game { struct World; }

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

        // we only need a minute of history
        ScrollingBuffer frequency = { 60 };
        ScrollingBuffer residency = { 60 };
    };

    struct RyzenMonitorDebug final : ServiceDebug {
        RyzenMonitorDebug();

        void draw() override;

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

    struct GdkDebug final : ServiceDebug {
        GdkDebug();

        void draw() override;
    };

    struct EngineDebug final : ServiceDebug {
        EngineDebug(game::World *pWorld);

        void draw() override;

    private:
        void drawFrameTimes();
        game::World *pWorld = nullptr;

        float lastUpdate = 0.f;
        float history = 10.f;
        debug::ScrollingBuffer frameTimes = { 4000 };

        float inputStep;
        float renderStep;
        float physicsStep;
        float gameStep;
    };

    struct ThreadServiceDebug final : ServiceDebug {
        ThreadServiceDebug();

        void draw() override;

    private:
        void drawPackage(threads::PackageIndex i);
        threads::CoreIndex getFastestCore(threads::ChipletIndex chiplet) const;

        const threads::Geometry& geometry;
    };
}
