#include "editor/ui/panels/audio.h"

#include "engine/audio/service.h"

using namespace editor;
using namespace editor::ui;

namespace {

}

AudioUi::AudioUi()
    : ServiceUi("Audio")
{ }

void AudioUi::draw() {
    openVorbisFile.Display();

    if (ImGui::Button("Open Audio file")) {
        openVorbisFile.SetTitle("Open Audio file");
        openVorbisFile.SetTypeFilters({ ".ogg" });
        openVorbisFile.Open();
    }

    if (openVorbisFile.HasSelected()) {
        auto path = openVorbisFile.GetSelected();
        auto pFileHandle = DepotService::openExternalFile(path);
        auto pSoundBuffer = audio::loadVorbisOgg(pFileHandle);
        buffers.push_back(pSoundBuffer);

        openVorbisFile.ClearSelected();
    }

    if (ImGui::BeginTable("Audio", 3)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Channels");
        ImGui::TableSetupColumn("Play");
        ImGui::TableHeadersRow();

        for (auto pSoundBuffer : buffers) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(pSoundBuffer->name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%zu", pSoundBuffer->channels);
            ImGui::TableNextColumn();
            if (ImGui::Button("Play")) {
                // ThreadService::enqueueWork("sound", [&, pSoundBuffer] {
                //     sounds.push_back(AudioService::playSound(pSoundBuffer));
                // });
            }
        }

        ImGui::EndTable();
    }
}
