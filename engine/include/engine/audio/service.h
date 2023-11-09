#pragma once

#include "engine/depot/service.h"

#include "engine/threads/mutex.h"

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
        static audio::SoundBufferPtr loadVorbisOgg(std::shared_ptr<depot::IFile> file);
        static audio::VoiceHandlePtr createVoice(std::string name, const audio::SoundFormat& format);

        static mt::SharedMutex& getBufferMutex();
        static std::vector<audio::SoundBufferPtr>& getBuffers();

        static mt::SharedMutex& getVoiceMutex();
        static std::vector<audio::VoiceHandlePtr>& getVoices();
    };
}
