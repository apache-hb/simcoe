#pragma once

#include "editor/debug/debug.h"
#include "engine/input/input.h"

using namespace editor;

namespace swarm {
    using namespace simcoe;
    using namespace simcoe::input;

    struct InputClient final : input::IClient {
        bool isShootPressed() const;
        bool isQuitPressed() const;

        bool consumeMoveUp();
        bool consumeMoveDown();
        bool consumeMoveLeft();
        bool consumeMoveRight();

        float getHorizontalAxis() const;
        float getVerticalAxis() const;

        float getButtonAxis(Button neg, Button pos) const;
        float getStickAxis(Axis axis) const;

        void onInput(const input::State& newState) override;

    private:
        void debug();
        debug::GlobalHandle debugHandle = debug::addGlobalHandle("Input", [this] { debug(); });

        Event shootKeyboardEvent;
        Event shootGamepadEvent;

        Event quitEventKey, quitEventGamepad;

        Event moveUpEventKey, moveUpEventArrow;
        Event moveDownEventKey, moveDownEventArrow;
        Event moveLeftEventKey, moveLeftEventArrow;
        Event moveRightEventKey, moveRightEventArrow;

        std::atomic_size_t updates = 0;

        input::State state;
    };

    InputClient *getInputClient();
}
