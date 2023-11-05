#pragma once

#include "editor/ui/service.h"
#include "editor/ui/components/buffer.h"

#include "game/tick.h"

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
        ScrollingBuffer tickTimes[game::eTickCount] = {};

        float inputStep;
        float renderStep;
        float physicsStep;
        float gameStep;
    };
}
