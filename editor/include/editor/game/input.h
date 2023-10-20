#pragma once

#include "editor/debug/debug.h"
#include "engine/input/input.h"

namespace editor::game {
    using namespace simcoe;
    using namespace simcoe::input;

    struct GameInputClient final : input::IClient {
        bool isShootPressed() const;

        float getHorizontalAxis() const;
        float getVerticalAxis() const;

        float getButtonAxis(Button neg, Button pos) const;
        float getStickAxis(Axis axis) const;

        void onInput(const input::State& newState) override;

    private:
        void debug();
        debug::UserHandle debugHandle = debug::addHandle("Input", [this] { debug(); });

        Event shootKeyboardEvent;
        Event shootGamepadEvent;

        std::atomic_size_t updates = 0;

        input::State state;
    };
}
