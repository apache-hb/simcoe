#pragma once

#include "engine/service/service.h"

#include "engine/depot/service.h"
#include "engine/threads/service.h"

#include "engine/render/render.h"
#include "engine/render/graph.h"

#include "engine/math/math.h"

#include "editor/debug/service.h"

#include "game/world.h"

namespace game {
    enum WindowMode : int {
        eModeWindowed,
        eModeBorderless,
        eModeFullscreen,

        eNone
    };

    struct GameService final : simcoe::IStaticService<GameService> {
        GameService();

        // IStaticService
        static constexpr std::string_view kServiceName = "game";
        static constexpr std::array kServiceDeps = {
            simcoe::DepotService::kServiceName,
            simcoe::ThreadService::kServiceName
        };

        // IService
        bool createService() override;
        void destroyService() override;

        static void start();

        // GameService
        static void shutdown() {
            get()->bWindowOpen = false;
            get()->pWorld->shutdown();
        }

        static bool shouldQuit() {
            return get()->pWorld->shouldQuit();
        }

        static void resizeDisplay(const WindowSize& event) {
            if (!get()->bWindowOpen) return;
            if (!get()->pWorld) return;

            get()->pWorld->pRenderQueue->add("resize", [event]() {
                get()->pGraph->resizeDisplay(event.width, event.height);
                LOG_INFO("resized display to: {}x{}", event.width, event.height);
            });
        }

        // editable stuff
        static WindowMode getWindowMode() {
            return get()->windowMode;
        }

        static void changeWindowMode(WindowMode newMode) {
            get()->pWorld->pRenderQueue->add("modechange", [newMode]() {
                get()->setWindowMode(getWindowMode(), newMode);
            });
        }

        static void changeInternalRes(const simcoe::math::uint2& newRes) {
            get()->pWorld->pRenderQueue->add("reschange", [newRes]() {
                get()->pGraph->resizeRender(newRes.width, newRes.height);
                LOG_INFO("changed internal resolution to: {}x{}", newRes.width, newRes.height);
            });
        }

        static void changeBackBufferCount(UINT newCount) {
            get()->pWorld->pRenderQueue->add("backbufferchange", [newCount]() {
                get()->pGraph->changeBackBufferCount(newCount);
                LOG_INFO("changed backbuffer count to: {}", newCount);
            });
        }

        static void changeCurrentAdapter(UINT newAdapter) {
            get()->pWorld->pRenderQueue->add("adapterchange", [newAdapter]() {
                get()->pGraph->changeAdapter(newAdapter);
                LOG_INFO("changed adapter to: {}", newAdapter);
            });
        }

        template<typename T, typename... A>
        static T *addDebugService(A&&... args) {
            auto *pService = new T(std::forward<A>(args)...);
            get()->debugServices.push_back(pService);
            return pService;
        }

        static std::span<editor::debug::ServiceDebug*> getDebugServices() {
            return get()->debugServices;
        }
    private:
        // render config
        size_t adapterIndex = 0;
        UINT backBufferCount = 2;
        simcoe::math::uint2 internalSize = { 1920 * 2, 1080 * 2 };
        size_t renderFaultLimit = 3;

        // game config
        size_t entityLimit = 0x1000;
        size_t seed = 0;

        void setWindowMode(WindowMode oldMode, WindowMode newMode);

        // render
        std::atomic_bool bWindowOpen = true;
        WindowMode windowMode = eModeWindowed;
        simcoe::render::Context *pContext = nullptr;
        simcoe::render::Graph *pGraph = nullptr;

        // game
        World *pWorld = nullptr;

        // threads
        threads::ThreadHandle *pRenderThread = nullptr;
        threads::ThreadHandle *pPhysicsThread = nullptr;
        threads::ThreadHandle *pGameThread = nullptr;

        std::vector<editor::debug::ServiceDebug*> debugServices;
    };
}
