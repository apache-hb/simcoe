#pragma once

#include "engine/core/win32.h"
#include "engine/depot/vfs.h"

#include <string>
#include <xaudio2.h>

namespace simcoe::audio {
    struct SoundBuffer {
        std::string name;

        size_t channels = 0;
        std::vector<uint8_t> data;

        WAVEFORMATEX format;
        XAUDIO2_BUFFER buffer;
    };

    struct SoundHandle {
        IXAudio2SourceVoice *pVoice = nullptr;
    };

    std::shared_ptr<SoundBuffer> loadVorbisOgg(std::shared_ptr<depot::IFile> file);
}
