#include "engine/rhi/service.h"

#include "engine/config/system.h"
#include "engine/core/error.h"
#include "engine/log/message.h"
#include "engine/log/service.h"

using namespace simcoe;
using namespace simcoe::rhi;

enum AdapterLookup : int64_t {
    eLookupDefault,
    eLookupSoftware,
    eLookupHighPerformance,
    eLookupLowPower,

    eLookupCount
};

const config::ConfigEnumMap kAdapterLookupMap = {
    { "default", eLookupDefault },
    { "software", eLookupSoftware },
    { "high_performance", eLookupHighPerformance },
    { "low_power", eLookupLowPower },
};

config::ConfigValue<UINT> cfgAdapterIndex("d3d12/adapter", "default_index", "The default adapter index to use (ignored it adapter_lookup is not default)", 0);
config::ConfigValue<AdapterLookup> cfgAdapterLookup("d3d12/adapter", "lookup_method", "The method to use to find the adapter", eLookupDefault, kAdapterLookupMap);

config::ConfigValue<bool> cfgEnableDebugLayer("d3d12/debug", "debug_layer", "Enable the d3d12 and dxgi debug layers", false);
config::ConfigValue<bool> cfgEnableInfoQueue("d3d12/debug", "info_queue", "Report errors via ID3D12InfoQueue1 if available", false);
config::ConfigValue<bool> cfgEnableDRED("d3d12/debug", "dred", "Enable device removed extended data (DRED)", false);

namespace {
    rhi::CreateFlags getCreateFlags(log::PendingMessage& msg) {
        rhi::CreateFlags flags = rhi::eCreateNone;

        bool bDebugLayer = cfgEnableDebugLayer.getCurrentValue();
        bool bInfoQueue = cfgEnableInfoQueue.getCurrentValue();
        bool bDRED = cfgEnableDRED.getCurrentValue();

        if (!bDebugLayer && !bDRED) {
            msg.addLine(
                "DRED enablement implies debug layer enablement\n"
                "enabling debug layer to satisfy DRED request"
            );
            
            bDebugLayer = true;
        }

        if (bDebugLayer) {
            flags |= rhi::eCreateDebug;
            msg.addLine("debug layer requested");
        }

        if (bInfoQueue) {
            flags |= rhi::eCreateInfoQueue;
            msg.addLine("info queue requested");
        }

        if (bDRED) {
            flags |= rhi::eCreateExtendedInfo;
            msg.addLine("DRED requested");
        }

        return flags;
    }

    // dxgi objects
    rhi::Context *gContext = nullptr;
    std::vector<rhi::Adapter*> gAdapters = {};


    bool bAdapterOwnedByList = false; /// TODO: this is such a shoddy workaround, ancestor cry
    rhi::Adapter *gSelectedAdapter = nullptr;

    rhi::Device *gDevice = nullptr;

    rhi::Adapter *selectAdapter(log::PendingMessage& msg) {
        auto method = cfgAdapterLookup.getCurrentValue();
        msg.addLine("looking for adapter via {}", kAdapterLookupMap.getKey(method));

        bAdapterOwnedByList = false;

        switch (method) {
        case eLookupDefault: {
            auto index = cfgAdapterIndex.getCurrentValue();
            msg.addLine("default adapter index: {}", index);
            if (index >= gAdapters.size()) {
                msg.addLine("default adapter index out of range, using first adapter");
                index = 0;
            }

            bAdapterOwnedByList = true;
            return gAdapters[index];
        }

        case eLookupSoftware: 
            return gContext->getWarpAdapter();
        
        case eLookupHighPerformance: 
            return gContext->getFastestAdapter();
        
        case eLookupLowPower: 
            return gContext->getLowPowerAdapter();
        
        default:
            core::throwFatal("invalid adapter lookup method: {}", int(method));
        }
    }
}

bool GpuService::createService() {
    log::PendingMessage msg("GpuService::createService");
    
    auto createFlags = getCreateFlags(msg);
    gContext = Context::create(createFlags);

    gAdapters = gContext->getAdapters();
    msg.addLine("found {} adapters", gAdapters.size());

    for (auto *pAdapter : gAdapters) {
        auto info = pAdapter->getInfo();
        msg.addLine("=== {} ({}) ===", info.name, info.type == rhi::AdapterType::eSoftware ? "software" : "hardware");
        msg.addLine("vendor = {} device = {} revision = {} subsystem = {}", info.vendorId, info.deviceId, info.revision, info.subsystemId);
        msg.addLine("video memory: {}", info.videoMemory.string());
        msg.addLine("system memory: {}", info.systemMemory.string());
        msg.addLine("shared memory: {}", info.sharedMemory.string());
    }

    gSelectedAdapter = selectAdapter(msg);
    auto info = gSelectedAdapter->getInfo();
    msg.addLine("using adapter: {}", info.name);

    gDevice = gSelectedAdapter->createDevice(createFlags);

    msg.send(log::eInfo);

    return true;
}

void GpuService::destroyService() {
    destroyDevice();

    if (!bAdapterOwnedByList)
        delete gSelectedAdapter;

    for (auto pAdapter : gAdapters) {
        delete pAdapter;
    }

    delete gContext;
}

rhi::Context *GpuService::getContext() {
    return gContext;
}

std::span<rhi::Adapter*> GpuService::getAdapters() {
    return gAdapters;
}

rhi::Adapter *GpuService::getSelectedAdapter() {
    return gSelectedAdapter;
}

rhi::Device *GpuService::getDevice() {
    if (gDevice == nullptr) {
        log::PendingMessage msg("GpuService::getDevice");

        gDevice = gSelectedAdapter->createDevice(getCreateFlags(msg));
        msg.send(log::eInfo);
    }

    return gDevice;
}

void GpuService::destroyDevice() {
    delete gDevice;
    gDevice = nullptr;
}

void GpuService::createDeviceAt(UINT adapterIndex) {
    log::PendingMessage msg("GpuService::createDeviceAt");

    auto createFlags = getCreateFlags(msg);
    msg.send(log::eInfo);

    bAdapterOwnedByList = true;
    gSelectedAdapter = gAdapters[adapterIndex];
    gDevice = gSelectedAdapter->createDevice(createFlags);
}

void GpuService::reportLiveObjects() {
    gContext->reportLiveObjects();
}

void GpuService::refreshAdapterList() {
    if (!bAdapterOwnedByList)
        delete gSelectedAdapter;

    for (auto pAdapter : gAdapters) {
        delete pAdapter;
    }
    gAdapters.clear();

    gAdapters = gContext->getAdapters();
    log::PendingMessage msg("GpuService::refreshAdapterList");
    gSelectedAdapter = selectAdapter(msg);
    msg.send(log::eInfo);
}
