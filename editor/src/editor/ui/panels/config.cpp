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

    void writeValue(const config::ConfigEntry *pEntry) {
        const void *pData = pEntry->getCurrentValue();
        config::ValueType type = pEntry->getType();

        switch (type) {
        case config::eConfigBool: {
            bool b;
            pEntry->saveValue(&b, sizeof(bool));
            ImGui::Text("%s", b ? "true" : "false");
            break;
        }
        case config::eConfigString: {
            const std::string& str = *static_cast<const std::string *>(pData);
            ImGui::TextUnformatted(str.c_str());
            break;
        }
        case config::eConfigInt: {
            int64_t i;
            pEntry->saveValue(&i, sizeof(int64_t));
            ImGui::Text("%lld", i);
            break;
        }
        case config::eConfigFloat: {
            float f = *static_cast<const float *>(pData);
            ImGui::Text("%f", f);
            break;
        }
        case config::eConfigEnum: {
            std::string name;
            pEntry->saveValue(&name, sizeof(std::string));
            ImGui::TextUnformatted(name.c_str());
            break;
        }

        default:
            ImGui::Text("unknown");
            break;
        }
    }

    void updateValue(config::ConfigEntry *pEntry) {
        switch (pEntry->getType()) {
        case config::eConfigBool: {
            bool b = *static_cast<const bool *>(pEntry->getCurrentValue());
            if (ImGui::Checkbox(pEntry->getName().data(), &b)) {
                pEntry->setCurrentValue(&b);
            }
            break;
        }
        case config::eConfigInt: {
            int i = *static_cast<const int *>(pEntry->getCurrentValue());
            if (ImGui::InputInt(pEntry->getName().data(), &i)) {
                int64_t i64 = i;
                pEntry->setCurrentValue(&i64);
            }
            break;
        }
        case config::eConfigFloat: {
            float f = *static_cast<const float *>(pEntry->getCurrentValue());
            if (ImGui::InputFloat(pEntry->getName().data(), &f)) {
                pEntry->setCurrentValue(&f);
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

        eSaveTotal
    };

    const auto kModeNames = std::array{
        "Save modified only",
        "Save all values"
    };
}

ConfigUi::ConfigUi()
    : ServiceUi("Config")
{ }

void ConfigUi::draw() {
    config::ConfigEntry *pRoot = config::getConfig();

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
            LOG_INFO("saving config to {}", saveConfigName.string());
            saveConfigFile.Close();

            ThreadService::enqueueWork("save-config", [=] {
                if (!ConfigService::saveConfig(saveConfigName, saveConfigType == eSaveUser)) {
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

void ConfigUi::drawConfigEntry(const std::string& id, config::ConfigEntry *pEntry) {
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
        writeValue(pEntry);

        ImGui::TableNextColumn();
        writeValue(pEntry);

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
