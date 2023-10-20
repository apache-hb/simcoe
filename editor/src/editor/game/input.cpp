#include "editor/game/input.h"

#include "imgui/imgui.h"

using namespace editor;
using namespace editor::game;

static constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV;

bool GameInputClient::isShootPressed() const {
    return shootKeyboardEvent.isPressed() || shootGamepadEvent.isPressed();
}

float GameInputClient::getHorizontalAxis() const {
    return getButtonAxis(Button::eKeyA, Button::eKeyD)
         + getButtonAxis(Button::eKeyLeft, Button::eKeyRight)
         + getStickAxis(Axis::eGamepadLeftX);
}

float GameInputClient::getVerticalAxis() const {
    return getButtonAxis(Button::eKeyS, Button::eKeyW)
         + getButtonAxis(Button::eKeyDown, Button::eKeyUp)
         + getStickAxis(Axis::eGamepadLeftY);
}

float GameInputClient::getButtonAxis(Button neg, Button pos) const {
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

float GameInputClient::getStickAxis(Axis axis) const {
    return state.axes[axis];
}

void GameInputClient::onInput(const input::State& newState) {
    state = newState;
    updates += 1;

    shootKeyboardEvent.update(state.buttons[Button::eKeySpace]);
    shootGamepadEvent.update(state.buttons[Button::ePadButtonDown]);
}

void GameInputClient::debug() {
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
