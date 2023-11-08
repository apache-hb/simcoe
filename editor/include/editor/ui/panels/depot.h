#pragma once

#include "editor/ui/service.h"

#include "engine/depot/vfs.h"

#include "imgui/imgui.h"
#include "imfiles/imfilebrowser.h"

namespace editor::ui {
    struct DepotUi final : ServiceUi {
        DepotUi();

        void draw() override;

    private:
        ImGui::FileBrowser openFile;

        std::vector<std::shared_ptr<simcoe::depot::IFile>> files;
    };
}
