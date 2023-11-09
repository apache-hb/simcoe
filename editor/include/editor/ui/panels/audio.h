#pragma once

#include "editor/ui/service.h"

#include "engine/audio/audio.h"

#include "imgui/imgui.h"
#include "imfiles/imfilebrowser.h"
#include <unordered_set>

template<>
struct std::less<simcoe::audio::SoundFormat> {
    bool operator()(const simcoe::audio::SoundFormat& lhs, const simcoe::audio::SoundFormat& rhs) const {
        // sort first by type, then by samples per second
        if (lhs.getFormatTag() != rhs.getFormatTag()) {
            return lhs.getFormatTag() < rhs.getFormatTag();
        }

        return lhs.getSamplesPerSecond() < rhs.getSamplesPerSecond();
    }
};

namespace editor::ui {
    struct AudioUi final : ServiceUi {
        AudioUi();

        void draw() override;

    private:
        ImGui::FileBrowser openVorbisFile;

        void drawBuffers();
        audio::SoundFormat selectedFormat;

        void drawVoices();
        audio::VoiceHandlePtr selectedVoice;
        audio::SoundBufferPtr selectedBuffer;

        void updateAvailableFormats();
        std::set<audio::SoundFormat> availableFormats;
    };
}
