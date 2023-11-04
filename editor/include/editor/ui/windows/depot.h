#pragma once

#include "editor/ui/ui.h"

#include "engine/depot/vfs.h"

#include "imgui/imgui.h"
#include "imfiles/imfilebrowser.h"

namespace editor::ui {
    struct DepotDebug final : ServiceDebug {
        DepotDebug();

        void draw() override;

    private:
        ImGui::FileBrowser openFile;

        std::vector<std::shared_ptr<simcoe::depot::IFile>> files;
    };
}
