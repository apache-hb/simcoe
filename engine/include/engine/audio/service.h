#pragma once

#include "engine/depot/service.h"

#include "engine/audio/audio.h"

namespace simcoe {
    struct AudioService final : IStaticService<AudioService> {
        // IStaticService
        constexpr static std::string_view kServiceName = "audio";
        constexpr static std::array<std::string_view, 0> kServiceDeps = { DepotService::kServiceName };

        // IService
        bool createService() override;
        void destroyService() override;

        // audio api
        static std::shared_ptr<audio::SoundBuffer> loadVorbisOgg(std::shared_ptr<depot::IFile> file);
        static std::shared_ptr<audio::VoiceHandle> createVoice();
    };
}
