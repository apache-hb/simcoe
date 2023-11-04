#pragma once

#include "editor/ui/ui.h"
#include "editor/ui/service.h"

namespace game { struct World; }

namespace editor::ui {
    struct EngineDebug final : ServiceDebug {
        EngineDebug(game::World *pWorld);

        void draw() override;

    private:
        void drawFrameTimes();
        game::World *pWorld = nullptr;

        float lastUpdate = 0.f;
        float history = 10.f;
        ScrollingBuffer frameTimes = { 4000 };

        float inputStep;
        float renderStep;
        float physicsStep;
        float gameStep;
    };
}
