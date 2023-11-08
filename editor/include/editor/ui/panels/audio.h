#pragma once

#include "editor/ui/service.h"

#include "engine/audio/service.h"

#include "imgui/imgui.h"
#include "imfiles/imfilebrowser.h"

namespace editor::ui {
    namespace a = simcoe::audio;
    struct AudioUi final : ServiceUi {
        AudioUi();

        void draw() override;

    private:
        ImGui::FileBrowser openVorbisFile;
        std::vector<a::SoundBufferPtr> buffers;

        std::vector<a::VoiceHandlePtr> voices;
    };
}
