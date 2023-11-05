#pragma once

#include "engine/service/service.h"

#include "engine/depot/service.h"
#include "engine/threads/service.h"

#include "engine/render/render.h"
#include "engine/render/graph.h"

#include "engine/math/math.h"

#include "editor/ui/service.h"

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
        static void shutdown();
        static bool shouldQuit();


        static void resizeDisplay(const WindowSize& event);

        // editable stuff
        static WindowMode getWindowMode();

        static void changeWindowMode(WindowMode newMode);
        static void changeInternalRes(const simcoe::math::uint2& newRes);
        static void changeBackBufferCount(UINT newCount);
        static void changeCurrentAdapter(UINT newAdapter);

        template<typename T, typename... A>
        static T *addDebugService(A&&... args) {
            auto *pService = new T(std::forward<A>(args)...);
            addDebugService(pService);
            return pService;
        }

        static std::span<editor::ui::ServiceDebug*> getDebugServices();

    private:
        static void addDebugService(editor::ui::ServiceDebug *pService);
    };
}
