#pragma once

#include "engine/core/win32.h"
#include "engine/depot/vfs.h"

#include <string>
#include <xaudio2.h>

#define XA_CHECK(EXPR) \
    do { \
        if (HRESULT hr = (EXPR); FAILED(hr)) { \
            core::throwNonFatal(#EXPR "\nxaudio2 error: {}", simcoe::audio::xaErrorString(hr)); \
        } \
    } while (false)

namespace simcoe::audio {
    std::string xaErrorString(HRESULT hr);

    enum SoundFormatTag : WORD {
        eFormatPcm = WAVE_FORMAT_PCM,
        eFormatWaveExtensible = WAVE_FORMAT_EXTENSIBLE,

        eFormatCount
    };

    struct SoundFormat {
        SoundFormat(WAVEFORMATEX format = {})
            : format(format)
        { }

        SoundFormatTag getFormatTag() const { return static_cast<SoundFormatTag>(format.wFormatTag); }
        WORD getChannels() const { return format.nChannels; }
        DWORD getSamplesPerSecond() const { return format.nSamplesPerSec; }
        WORD getBitsPerSample() const { return format.wBitsPerSample; }

        const WAVEFORMATEX *getFormat() const { return &format; }

        constexpr bool operator==(const SoundFormat& rhs) const {
            return format.wFormatTag == rhs.format.wFormatTag
                && format.nChannels == rhs.format.nChannels
                && format.nSamplesPerSec == rhs.format.nSamplesPerSec
                && format.wBitsPerSample == rhs.format.wBitsPerSample;
        }

        constexpr bool operator!=(const SoundFormat& rhs) const {
            return format.wFormatTag != rhs.format.wFormatTag
                || format.nChannels != rhs.format.nChannels
                || format.nSamplesPerSec != rhs.format.nSamplesPerSec
                || format.wBitsPerSample != rhs.format.wBitsPerSample;
        }

    private:
        WAVEFORMATEX format;
    };

    struct SoundBuffer {
        SoundBuffer(std::string name, std::vector<uint8_t> bytes, SoundFormat format);
        ~SoundBuffer();

        const char *getName() const { return name.c_str(); }

        const SoundFormat& getFormat() const { return format; }
        const XAUDIO2_BUFFER *getBuffer() const { return &buffer; }

    private:
        std::string name;

        size_t channels;
        std::vector<uint8_t> data;

        SoundFormat format;
        XAUDIO2_BUFFER buffer = {};
    };

    struct VoiceHandle {
        VoiceHandle(std::string name, SoundFormat format, IXAudio2SourceVoice *pVoice)
            : name(std::move(name))
            , format(format)
            , pVoice(pVoice)
        { }

        ~VoiceHandle();

        void submit(std::shared_ptr<SoundBuffer> buffer);
        void pause();
        void resume();
        void reset();

        void stop() {
            pause();
            reset();
        }

        float getVolume() const;
        void setVolume(float volume);

        const char *getName() const { return name.c_str(); }

        const SoundFormat& getFormat() const { return format; }

    private:
        std::string name;

        SoundFormat format;
        IXAudio2SourceVoice *pVoice = nullptr;
    };

    using SoundBufferPtr = std::shared_ptr<SoundBuffer>;
    using VoiceHandlePtr = std::shared_ptr<VoiceHandle>;

    SoundBufferPtr loadVorbisOgg(std::shared_ptr<depot::IFile> file);
}
