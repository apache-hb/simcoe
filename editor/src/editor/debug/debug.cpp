#include "editor/debug/debug.h"

#include "imgui/imgui.h"
#include <unordered_set>

using namespace simcoe;

using namespace editor;
using namespace editor::debug;

namespace {
    std::mutex gLock;
    std::unordered_set<debug::DebugHandle*> gHandles;
}

UserHandle debug::addHandle(const std::string &name, std::function<void ()> draw) {
    DebugHandle *pHandle = new DebugHandle{ name, draw };
    UserHandle userHandle(pHandle, [](auto *pHandle) {
        debug::removeHandle(pHandle);
    });

    std::lock_guard lock(gLock);
    gHandles.insert(pHandle);

    return userHandle;
}

void debug::removeHandle(DebugHandle *pHandle) {
    std::lock_guard lock(gLock);
    gHandles.erase(pHandle);
    delete pHandle;
}

void debug::enumHandles(std::function<void(DebugHandle*)> callback) {
    std::lock_guard lock(gLock);
    for (auto *pHandle : gHandles) {
        callback(pHandle);
    }
}

static void showHeapSlots(const char *name, const simcoe::BitMap& alloc) {
    if (ImGui::CollapsingHeader(name)) {
        // show a grid of slots
        auto size = alloc.getSize();
        auto rows = size / 8;
        auto cols = size / rows;

        if (ImGui::BeginTable("Slots", cols)) {
            for (auto i = 0; i < size; i++) {
                ImGui::TableNextColumn();
                if (alloc.test(BitMap::Index(i))) {
                    ImGui::Text("%d (used)", i);
                } else {
                    ImGui::TextDisabled("%d (free)", i);
                }
            }

            ImGui::EndTable();
        }
    }
}

template<typename T, typename F>
static void showGraphObjects(const char *name, const std::vector<T>& objects, F&& showObject) {
    if (ImGui::CollapsingHeader(name)) {
        for (auto& object : objects) {
            showObject(object);
        }
    }
}

void debug::showDebugGui(render::Graph *pGraph) {
    render::Context *pContext = pGraph->getContext();
    if (ImGui::Begin("Render Debug")) {
        auto *pRtvHeap = pContext->getRtvHeap();
        auto *pDsvHeap = pContext->getDsvHeap();
        auto *pSrvHeap = pContext->getSrvHeap();

        auto& rtvAlloc = pRtvHeap->allocator;
        auto& dsvAlloc = pDsvHeap->allocator;
        auto& srvAlloc = pSrvHeap->allocator;

        showHeapSlots(std::format("RTV heap {}", rtvAlloc.getSize()).c_str(), rtvAlloc);
        showHeapSlots(std::format("DSV heap {}", dsvAlloc.getSize()).c_str(), dsvAlloc);
        showHeapSlots(std::format("SRV heap {}", srvAlloc.getSize()).c_str(), srvAlloc);
    }
    ImGui::End();

    if (ImGui::Begin("Graph Debug")) {
        const auto& resources = pGraph->resources;
        const auto& passes = pGraph->passes;
        const auto& objects = pGraph->objects;

        showGraphObjects(std::format("resources: {}", resources.size()).c_str(), resources, [](render::IResourceHandle *pResource) {
            auto name = pResource->getName();
            ImGui::Text("%s (state: %s)", name.data(), rhi::toString(pResource->getCurrentState()).data());
        });

        showGraphObjects(std::format("passes: {}", passes.size()).c_str(), passes, [](render::ICommandPass *pPass) {
            auto name = pPass->getName();
            ImGui::Text("pass: %s", name.data());
            for (auto& resource : pPass->inputs) {
                auto handle = resource->getResourceHandle();
                auto state = resource->getRequiredState();
                ImGui::BulletText("resource: %s (expected: %s)", handle->getName().data(), rhi::toString(state).data());
            }
        });

        showGraphObjects(std::format("objects: {}", objects.size()).c_str(), objects, [](render::IGraphObject *pObject) {
            auto name = pObject->getName();
            ImGui::Text("%s", name.data());
        });
    }
    ImGui::End();
}
