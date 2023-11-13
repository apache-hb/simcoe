#pragma once

#include "engine/config/service.h"

#include "engine/rhi/rhi.h"

namespace simcoe {
    struct GpuService final : IStaticService<GpuService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "rhi";
        static inline auto kServiceDeps = depends(ConfigService::service());

        // IService
        bool createService() override;
        void destroyService() override;

        // public api
        static rhi::Context *getContext();
        static std::span<rhi::Adapter*> getAdapters();
        static rhi::Adapter *getSelectedAdapter();

        // dont `delete` this, use `destroyDevice` instead
        static rhi::Device *getDevice();
        static void destroyDevice();
        static void createDeviceAt(UINT adapterIndex);

        // context stuff
        static void reportLiveObjects();
        static void refreshAdapterList();
    };
}
