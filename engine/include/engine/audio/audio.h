#pragma once

#include "engine/core/win32.h"
#include "engine/depot/vfs.h"

#include <string>
#include <xaudio2.h>

namespace simcoe::audio {
    struct SoundFormat {
        WAVEFORMATEX format;
    };

    struct SoundBuffer {
        std::string name;

        size_t channels = 0;
        std::vector<uint8_t> data;

        WAVEFORMATEX format;
        XAUDIO2_BUFFER buffer;
    };

    struct VoiceHandle {
        void play(std::shared_ptr<SoundBuffer> buffer);
        void stop();

        IXAudio2SourceVoice *pVoice = nullptr;
    };

    using SoundBufferPtr = std::shared_ptr<SoundBuffer>;
    using VoiceHandlePtr = std::shared_ptr<VoiceHandle>;

    SoundBufferPtr loadVorbisOgg(std::shared_ptr<depot::IFile> file);
}
