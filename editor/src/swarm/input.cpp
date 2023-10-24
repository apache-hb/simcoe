#include "swarm/input.h"

#include "imgui/imgui.h"

using namespace swarm;

static constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV;

bool InputClient::isShootPressed() const {
    return shootKeyboardEvent.isPressed()
        || shootGamepadEvent.isPressed();
}

bool InputClient::isQuitPressed() const {
    return quitEventKey.isPressed()
        || quitEventGamepad.isPressed();
}

bool InputClient::consumeMoveUp() {
    return moveUpEventKey.beginPress()
        || moveUpEventArrow.beginPress()
        || moveUpEventPad.beginPress();
}

bool InputClient::consumeMoveDown() {
    return moveDownEventKey.beginPress()
        || moveDownEventArrow.beginPress()
        || moveDownEventPad.beginPress();
}

bool InputClient::consumeMoveLeft() {
    return moveLeftEventKey.beginPress()
        || moveLeftEventArrow.beginPress()
        || moveLeftEventPad.beginPress();
}

bool InputClient::consumeMoveRight() {
    return moveRightEventKey.beginPress()
        || moveRightEventArrow.beginPress()
        || moveRightEventPad.beginPress();
}

float InputClient::getHorizontalAxis() const {
    return getButtonAxis(Button::eKeyA, Button::eKeyD)
         + getButtonAxis(Button::eKeyLeft, Button::eKeyRight)
         + getStickAxis(Axis::eGamepadLeftX);
}

float InputClient::getVerticalAxis() const {
    return getButtonAxis(Button::eKeyS, Button::eKeyW)
         + getButtonAxis(Button::eKeyDown, Button::eKeyUp)
         + getStickAxis(Axis::eGamepadLeftY);
}

float InputClient::getButtonAxis(Button neg, Button pos) const {
    size_t negIdx = state.buttons[neg];
    size_t posIdx = state.buttons[pos];

    if (negIdx > posIdx) {
        return -1.f;
    } else if (posIdx > negIdx) {
        return 1.f;
    } else {
        return 0.f;
    }
}

float InputClient::getStickAxis(Axis axis) const {
    return state.axes[axis];
}

void InputClient::onInput(const input::State& newState) {
    state = newState;
    updates += 1;

    quitEventKey.update(state.buttons[Button::eKeyEscape]);
    quitEventGamepad.update(state.buttons[Button::ePadBack]);

    shootKeyboardEvent.update(state.buttons[Button::eKeySpace]);
    shootGamepadEvent.update(state.buttons[Button::ePadButtonDown]);

    moveUpEventKey.update(state.buttons[Button::eKeyW]);
    moveDownEventKey.update(state.buttons[Button::eKeyS]);
    moveLeftEventKey.update(state.buttons[Button::eKeyA]);
    moveRightEventKey.update(state.buttons[Button::eKeyD]);

    moveUpEventArrow.update(state.buttons[Button::eKeyUp]);
    moveDownEventArrow.update(state.buttons[Button::eKeyDown]);
    moveLeftEventArrow.update(state.buttons[Button::eKeyLeft]);
    moveRightEventArrow.update(state.buttons[Button::eKeyRight]);

    moveUpEventPad.update(state.buttons[Button::ePadDirectionUp]);
    moveDownEventPad.update(state.buttons[Button::ePadDirectionDown]);
    moveLeftEventPad.update(state.buttons[Button::ePadDirectionLeft]);
    moveRightEventPad.update(state.buttons[Button::ePadDirectionRight]);
}

void InputClient::debug() {
    ImGui::Text("updates: %zu", updates.load());
    ImGui::Text("device: %s", input::toString(state.device).data());
    ImGui::SeparatorText("buttons");

    if (ImGui::BeginTable("buttons", 2, kTableFlags)) {
        ImGui::TableNextColumn();
        ImGui::Text("button");
        ImGui::TableNextColumn();
        ImGui::Text("state");

        for (size_t i = 0; i < state.buttons.size(); i++) {
            ImGui::TableNextColumn();
            ImGui::Text("%s", input::toString(input::Button(i)).data());
            ImGui::TableNextColumn();
            ImGui::Text("%zu", state.buttons[i]);
        }

        ImGui::EndTable();
    }

    ImGui::SeparatorText("axes");
    if (ImGui::BeginTable("axes", 2, kTableFlags)) {
        ImGui::TableNextColumn();
        ImGui::Text("axis");
        ImGui::TableNextColumn();
        ImGui::Text("value");

        for (size_t i = 0; i < state.axes.size(); i++) {
            ImGui::TableNextColumn();
            ImGui::Text("%s", input::toString(input::Axis(i)).data());
            ImGui::TableNextColumn();
            ImGui::Text("%f", state.axes[i]);
        }

        ImGui::EndTable();
    }
}

InputClient *swarm::getInputClient() {
    static InputClient client;
    return &client;
}
