#include "editor/ui/panels/audio.h"

#include "engine/audio/service.h"

#include "imgui/imgui_internal.h"

using namespace editor;
using namespace editor::ui;

namespace {
    const char *getFormatTagName(audio::SoundFormatTag tag) {
        switch (tag) {
        case audio::eFormatPcm: return "PCM";
        case audio::eFormatWaveExtensible: return "Wave Extensible";
        default: return nullptr;
        }
    }

    void formatSoundFormat(char *label, size_t len, const audio::SoundFormat& fmt) {
        auto tag = fmt.getFormatTag();
        if (const char *pFormatTag = getFormatTagName(tag)) {
            sprintf_s(label, len, "%s (%d channels, %lu samples/sec, %d bits/sample)", pFormatTag, fmt.getChannels(), fmt.getSamplesPerSecond(), fmt.getBitsPerSample());
        } else {
            sprintf_s(label, len, "Unknown (%d channels, %lu samples/sec, %d bits/sample)", fmt.getChannels(), fmt.getSamplesPerSecond(), fmt.getBitsPerSample());
        }
    }

    const ImGuiTableFlags kFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;

    const char *pCreateVoiceId = "Create Voice";
    const char *pPlaySoundId = "Play Sound";
    const ImGuiID kSoundPopupId = ImHashStr(pPlaySoundId);
}

AudioUi::AudioUi()
    : ServiceUi("Audio")
{ }

void AudioUi::draw() {
    updateAvailableFormats();

    openVorbisFile.Display();

    if (ImGui::Button("Open Audio file")) {
        openVorbisFile.SetTitle("Open Audio file");
        openVorbisFile.SetTypeFilters({ ".ogg" });
        openVorbisFile.Open();
    }

    if (openVorbisFile.HasSelected()) {
        auto path = openVorbisFile.GetSelected();

        ThreadService::enqueueWork("load-ogg", [path] {
            auto pFileHandle = DepotService::openExternalFile(path);
            AudioService::loadVorbisOgg(pFileHandle);
        });

        openVorbisFile.ClearSelected();
    }

    drawBuffers();
    drawVoices();
}

void AudioUi::drawBuffers() {
    if (ImGui::BeginTable("Buffers", 3, kFlags)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Format", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        auto& bufferMutex = AudioService::getBufferMutex();
        auto& soundBuffers = AudioService::getBuffers();

        for (auto pSoundBuffer : mt::roIter(bufferMutex, soundBuffers)) {
            ImGui::TableNextRow();

            // name
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(pSoundBuffer->getName());

            // format
            ImGui::TableNextColumn();
            const auto& format = pSoundBuffer->getFormat();
            auto tag = format.getFormatTag();
            if (const char *pFormatTag = getFormatTagName(tag)) {
                ImGui::TextUnformatted(pFormatTag);
            } else {
                ImGui::Text("Unknown (%d)", WORD(tag));
            }

            // info

            ImGui::TableNextColumn();
            auto channels = format.getChannels();
            auto samplesPerSec = format.getSamplesPerSecond();
            auto bitsPerSample = format.getBitsPerSample();

            ImGui::Text("channels: %d\nsamples/second: %lu\nbits/sample: %d", channels, samplesPerSec, bitsPerSample);
        }

        ImGui::EndTable();
    }

    static int selectedIndex = INT_MAX;

    if (ImGui::BeginPopup(pCreateVoiceId)) {
        // create a voice based off a format we get from a buffer
        int i = 0;

        char selected[128] = "";
        if (selectedIndex != INT_MAX)
            formatSoundFormat(selected, sizeof(selected), selectedFormat);

        if (ImGui::BeginCombo("Format", selected)) {
            for (const auto& format : availableFormats) {
                char label[128];
                formatSoundFormat(label, sizeof(label), format);

                bool bIsSelected = selectedIndex == i;
                if (ImGui::Selectable(label, bIsSelected)) {
                    selectedFormat = format;
                    selectedIndex = i;
                }

                if (bIsSelected)
                    ImGui::SetItemDefaultFocus();

                i += 1;
            }

            ImGui::EndCombo();
        }

        if (ImGui::Button("Create")) {
            selectedIndex = INT_MAX;

            ThreadService::enqueueWork("new-voice", [=] {
                AudioService::createVoice("voice", selectedFormat);
            });
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (ImGui::Button(pCreateVoiceId))
        ImGui::OpenPopup(pCreateVoiceId);
}

void AudioUi::drawVoices() {
    ImGui::PushOverrideID(kSoundPopupId);
    if (ImGui::BeginPopup(pPlaySoundId)) {

        if (ImGui::BeginCombo("Sound", selectedBuffer ? selectedBuffer->getName() : "")) {
            auto& bufferMutex = AudioService::getBufferMutex();
            auto& soundBuffers = AudioService::getBuffers();
            mt::read_lock lock(bufferMutex);

            for (size_t i = 0; i < soundBuffers.size(); i++) {
                auto pSoundBuffer = soundBuffers[i];
                if (pSoundBuffer->getFormat() != selectedVoice->getFormat())
                    continue;

                bool bIsSelected = selectedBuffer == pSoundBuffer;
                if (ImGui::Selectable(pSoundBuffer->getName(), bIsSelected)) {
                    selectedBuffer = pSoundBuffer;
                }

                if (bIsSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (ImGui::Button("Play")) {
            selectedVoice->submit(selectedBuffer);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    ImGui::PopID();

    if (ImGui::BeginTable("Voices", 4, kFlags)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Format", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        auto& voiceMutex = AudioService::getVoiceMutex();
        auto& voices = AudioService::getVoices();

        for (auto pVoice : mt::rwIter(voiceMutex, voices)) {
            ImGui::TableNextRow();

            // name
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(pVoice->getName());

            // format
            ImGui::TableNextColumn();
            const auto& format = pVoice->getFormat();
            char label[128];
            formatSoundFormat(label, sizeof(label), format);
            ImGui::TextUnformatted(label);

            // volume

            // make the slider expand to available space
            ImGui::TableNextColumn();

            ImGui::PushItemWidth(-1);
            float volume = pVoice->getVolume();
            if (ImGui::SliderFloat("##volume", &volume, 0.0f, 1.0f, "%.2f")) {
                pVoice->setVolume(volume);
            }
            ImGui::PopItemWidth();

            // actions
            ImGui::TableNextColumn();
            if (ImGui::Button("Play")) {
                selectedVoice = pVoice;
                ImGui::PushOverrideID(kSoundPopupId);
                ImGui::OpenPopup(pPlaySoundId, ImGuiPopupFlags_AnyPopup);
                ImGui::PopID();
            }

            ImGui::SameLine();
            if (ImGui::Button("Stop")) {
                pVoice->stop();
            }

            if (ImGui::Button("Pause")) { pVoice->pause(); }

            ImGui::SameLine();
            if (ImGui::Button("Resume")) { pVoice->resume(); }
        }
        ImGui::EndTable();
    }
}

void AudioUi::updateAvailableFormats() {
    auto& bufferMutex = AudioService::getBufferMutex();
    auto& soundBuffers = AudioService::getBuffers();

    availableFormats.clear();

    for (auto pSoundBuffer : mt::roIter(bufferMutex, soundBuffers)) {
        availableFormats.insert(pSoundBuffer->getFormat());
    }
}
