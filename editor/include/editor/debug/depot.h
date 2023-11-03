#pragma once

#include "editor/debug/debug.h"

#include "engine/depot/vfs.h"

#include "imgui/imgui.h"
#include "imfiles/imfilebrowser.h"

namespace editor::debug {
    struct DepotDebug final : ServiceDebug {
        DepotDebug();

        void draw() override;

    private:
        ImGui::FileBrowser openFile;

        std::vector<std::shared_ptr<simcoe::depot::IFile>> files;
    };
}
