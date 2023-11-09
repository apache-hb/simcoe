#include "editor/ui/panels/config.h"

#include "engine/threads/service.h"
#include "engine/config/service.h"

#include "imgui/imgui_internal.h"

using namespace editor;
using namespace editor::ui;

using simcoe::ThreadService;

namespace {
    constexpr const char *getConfigTypeName(config::ValueType type) {
        switch (type) {
        case config::eConfigBool: return "bool";
        case config::eConfigString: return "string";
        case config::eConfigInt: return "int";
        case config::eConfigFloat: return "float";
        case config::eConfigEnum: return "enum";
        case config::eConfigFlags: return "flags";
        default: return "unknown";
        }
    }

    void drawValue(const config::IConfigEntry *pEntry) {
        config::ValueType type = pEntry->getType();

        switch (type) {
        case config::eConfigBool: {
            bool b;
            pEntry->unparseCurrentValue(&b, sizeof(bool));
            ImGui::Text("%s", b ? "true" : "false");
            break;
        }
        case config::eConfigInt: {
            int64_t i;
            pEntry->unparseCurrentValue(&i, sizeof(int64_t));
            ImGui::Text("%lld", i);
            break;
        }
        case config::eConfigFloat: {
            float f;
            pEntry->unparseCurrentValue(&f, sizeof(float));
            ImGui::Text("%f", f);
            break;
        }
        case config::eConfigEnum:
        case config::eConfigFlags: {
            std::string str;
            pEntry->unparseCurrentValue(&str, sizeof(std::string));
            ImGui::TextUnformatted(str.data());
            break;
        }

        case config::eConfigString: {
            std::string str;
            pEntry->unparseCurrentValue(&str, sizeof(std::string));
            ImGui::TextUnformatted(str.data());
            break;
        }

        default:
            ImGui::TextDisabled("---");
            break;
        }
    }

    void updateValue(config::IConfigEntry *pEntry) {
        switch (pEntry->getType()) {
        case config::eConfigBool: {
            bool b;
            pEntry->unparseCurrentValue(&b, sizeof(bool));
            if (ImGui::Checkbox(pEntry->getName().data(), &b)) {
                pEntry->parseValue(&b, sizeof(bool));
            }
            break;
        }
        case config::eConfigInt: {
            int i;
            pEntry->unparseCurrentValue(&i, sizeof(int64_t));
            if (ImGui::InputInt(pEntry->getName().data(), &i)) {
                int64_t i64 = i;
                pEntry->parseValue(&i64, sizeof(int64_t));
            }
            break;
        }
        case config::eConfigFloat: {
            float f;
            pEntry->unparseCurrentValue(&f, sizeof(float));
            if (ImGui::InputFloat(pEntry->getName().data(), &f)) {
                pEntry->parseValue(&f, sizeof(float));
            }
            break;
        }

        default:
            ImGui::Text("unknown");
            break;
        }
    }

    constexpr auto kTableFlags = ImGuiTableFlags_BordersV
                               | ImGuiTableFlags_BordersOuterH
                               | ImGuiTableFlags_Resizable
                               | ImGuiTableFlags_RowBg
                               | ImGuiTableFlags_NoBordersInBody;

    constexpr auto kGroupNodeFlags = ImGuiTreeNodeFlags_SpanAllColumns;

    constexpr auto kValueNodeFlags = kGroupNodeFlags
                                   | ImGuiTreeNodeFlags_Leaf
                                   | ImGuiTreeNodeFlags_Bullet
                                   | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    enum SaveMode {
        eSaveUser, // save only user modified values
        eSaveAll, // save all values, including defaults
        eSaveDefaults, // write all default values to file

        eSaveTotal
    };

    const auto kModeNames = std::array{
        "Save modified only", // useful for creating user configs
        "Save all values", // for creating global configs
        "Save default values only" // for listing all default values
    };
}

ConfigUi::ConfigUi()
    : ServiceUi("Config")
{ }

void ConfigUi::draw() {
    config::IConfigEntry *pRoot = config::getConfig();

    if (ImGui::Button("Save config")) {
        saveConfigFile.SetTitle("Save Config File");
        saveConfigFile.SetTypeFilters({ ".toml" });
        saveConfigFile.Open();
    }

    ImGui::SameLine();

    if (ImGui::Button("Load config")) {
        loadConfigFile.SetTitle("Load Config File");
        loadConfigFile.SetTypeFilters({ ".toml" });
        loadConfigFile.Open();
    }

    if (ImGui::BeginTable("Config", 5, kTableFlags)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 150.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 70.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 50.f);
        ImGui::TableSetupColumn("Default", ImGuiTableColumnFlags_WidthStretch, 50.f);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableHeadersRow();

        for (const auto& [name, pEntry] : pRoot->getChildren()) {
            drawConfigEntry(name, pEntry);
        }

        ImGui::EndTable();
    }

    saveConfigFile.Display();
    loadConfigFile.Display();

    if (ImGui::BeginPopup("Save config")) {
        ImGui::Combo("Mode", &saveConfigType, kModeNames.data(), int(kModeNames.size()));
        if (ImGui::Button("Confirm")) {
            ImGui::CloseCurrentPopup();
            saveConfigFile.Close();

            ThreadService::enqueueWork("save-config", [=] {
                bool bResult = false;
                switch (saveConfigType) {
                case eSaveUser:
                    bResult = ConfigService::saveConfig(saveConfigName, true);
                    break;
                case eSaveAll:
                    bResult = ConfigService::saveConfig(saveConfigName, false);
                    break;
                case eSaveDefaults:
                    bResult = ConfigService::saveDefaultConfig(saveConfigName);
                    break;
                default:
                    SM_NEVER("invalid save config type {}", saveConfigType);
                }

                if (!bResult) {
                    LOG_WARN("failed to save config to {}", saveConfigName.string());
                } else {
                    LOG_INFO("saved config to {}", saveConfigName.string());
                }
            });
        }
        ImGui::EndPopup();
    }

    if (saveConfigFile.HasSelected()) {
        saveConfigName = saveConfigFile.GetSelected();
        ImGui::OpenPopup("Save config");
    }
}

void ConfigUi::drawConfigEntry(const std::string& id, config::IConfigEntry *pEntry) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    if (pEntry->getType() == config::eConfigGroup) {
        bool bGroupOpen = ImGui::TreeNodeEx(id.c_str(), kGroupNodeFlags);

        ImGui::TableNextColumn();
        ImGui::TextDisabled("--");

        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        if (bGroupOpen) {
            for (const auto& [name, pChild] : pEntry->getChildren()) {
                drawConfigEntry(name, pChild);
            }

            ImGui::TreePop();
        }
    } else {
        ImGui::TreeNodeEx(id.c_str(), kValueNodeFlags);
        ImGui::TableNextColumn();
        ImGui::Text("%s", getConfigTypeName(pEntry->getType()));

        ImGui::TableNextColumn();
        drawValue(pEntry);

        ImGui::TableNextColumn();
        drawValue(pEntry);

        ImGui::TableNextColumn();
        ImGui::Text("%s", pEntry->getDescription().data());

        if (ImGui::BeginPopup(id.c_str())) {
            updateValue(pEntry);
            ImGui::EndPopup();
        }

        // if the row is clicked, open the popup
        if (ImGui::TableGetHoveredRow() == ImGui::TableGetRowIndex() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup(id.c_str());
        }
    }
}
