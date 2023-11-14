#pragma once

#include "engine/render/render.h"
#include "engine/render/graph.h"

#include "engine/rhi/service.h"

namespace simcoe {
    struct RenderService final : IStaticService<RenderService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "render";
        static inline auto kServiceDeps = depends(GpuService::service());

        // IService
        bool createService() override;
        void destroyService() override;

        // RenderService
        static render::Graph *getGraph();
        static void start();
        static void shutdown();

        static void enqueueWork(std::string name, threads::WorkItem&& work);

        static void draw();

        static void changeAdapter(UINT newAdapter);
        static void changeBackBufferCount(UINT newCount);
        static void resizeDisplay(const WindowSize& event); // change display size
        static void resizeRender(const math::uint2& event); // change render size

        static void setFullscreen(bool bFullscreen);
    };
}
