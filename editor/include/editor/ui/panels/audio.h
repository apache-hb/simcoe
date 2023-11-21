#pragma once

#include "editor/ui/service.h"

#include "engine/audio/audio.h"

#include "imgui/imgui.h"
#include "imfiles/imfilebrowser.h"
#include <unordered_set>

namespace editor::ui {
    namespace audio = simcoe::audio;
    
    struct AudioUi final : ServiceUi {
        AudioUi();

        void draw() override;

    private:
        ImGui::FileBrowser openVorbisFile{ImGuiFileBrowserFlags_MultipleSelection};
        ImGuiTextFilter bufferSearchFilter;

        void drawBuffers();
        audio::SoundFormat selectedFormat;

        void drawVoices();
        audio::VoiceHandlePtr selectedVoice;
        audio::SoundBufferPtr selectedBuffer;

        void updateAvailableFormats();
        std::set<audio::SoundFormat> availableFormats;
    };
}
