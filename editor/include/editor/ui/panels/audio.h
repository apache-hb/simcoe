#pragma once

#include "editor/ui/service.h"

#include "engine/audio/service.h"

#include "imgui/imgui.h"
#include "imfiles/imfilebrowser.h"

namespace editor::ui {
    struct AudioUi final : ServiceUi {
        AudioUi();

        void draw() override;

    private:
        ImGui::FileBrowser openVorbisFile;
        std::vector<std::shared_ptr<simcoe::audio::SoundBuffer>> buffers;

        std::vector<std::shared_ptr<simcoe::audio::SoundHandle>> sounds;
    };
}
