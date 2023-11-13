#pragma once

#include "editor/ui/service.h"

#include "engine/config/config.h"

#include "imgui/imgui.h"
#include "imfiles/imfilebrowser.h"

namespace editor::ui {
    struct ConfigUi final : ServiceUi {
        ConfigUi();

        void draw() override;

    private:
        ImGui::FileBrowser loadConfigFile;
        ImGui::FileBrowser saveConfigFile{ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CreateNewDir};

        int saveConfigType = 0;
        fs::path saveConfigName;

        void drawConfigEntry(const std::string& name, config::IConfigEntry *pEntry);
    };
}
