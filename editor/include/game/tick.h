#pragma once

namespace game {
    enum ThreadTick {
        eTickRender,
        eTickPhysics,
        eTickGame,

        eTickCount
    };

    struct TickAlert {
        ThreadTick tick;
        float time;
        float delta;
    };
}