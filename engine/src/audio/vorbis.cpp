#include "engine/audio/audio.h"
#include "engine/audio/service.h"

#include "engine/core/error.h"
#include "engine/core/units.h"
#include "engine/log/message.h"
#include "engine/log/service.h"

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

using namespace simcoe;
using namespace simcoe::audio;

size_t ovRead(void *ptr, size_t size, size_t nmemb, void *data) {
    size_t bytes = size * nmemb;
    auto file = static_cast<depot::IFile *>(data);
    return file->read(ptr, bytes);
}

depot::SeekMode remapWhence(int whence) {
    switch (whence) {
    case SEEK_SET: return depot::eAbsolute;
    case SEEK_CUR: return depot::eCurrent;
    case SEEK_END: return depot::eEnd;
    default: core::throwFatal("invalid whence: {}", whence);
    }
}

int ovSeek(void *data, ogg_int64_t offset, int whence) {
    auto file = static_cast<depot::IFile *>(data);
    return core::intCast<int>(file->seek(offset, remapWhence(whence)));
}

long ovTell(void *data) {
    auto file = static_cast<depot::IFile *>(data);
    return core::intCast<long>(file->tell());
}

const ov_callbacks kVorbisCallbacks = {
    .read_func = ovRead,
    .seek_func = ovSeek,
    .close_func = nullptr, // we don't own the file
    .tell_func = ovTell
};

std::shared_ptr<SoundBuffer> audio::loadVorbisOgg(std::shared_ptr<depot::IFile> file) {
    depot::IFile *pImpl = file.get();
    OggVorbis_File vf;

    if (int err = ov_open_callbacks((void*)pImpl, &vf, nullptr, 0, kVorbisCallbacks); err < 0) {
        LOG_WARN("failed to open ogg file: {} (error = {})", file->getName(), err);
        return {};
    }

    vorbis_comment *pComment = ov_comment(&vf, -1);
    vorbis_info *pInfo = ov_info(&vf, -1);
    long bitrate = ov_bitrate(&vf, -1);

    log::PendingMessage msg{"vorbis ogg"};
    msg.addLine("channels: {} rate: {}", pInfo->channels, pInfo->rate);
    msg.addLine("vendor: {}", pComment->vendor);
    msg.addLine("bitrate: {}", bitrate);

    for (int i = 0; i < pComment->comments; i++) {
        msg.addLine("comment: {}", pComment->user_comments[i]);
    }

    msg.send(log::eDebug);

    WORD wChannels = core::intCast<WORD>(pInfo->channels);
    DWORD nSamplesPerSec = core::intCast<DWORD>(pInfo->rate);
    WORD nBlockAlign = core::intCast<WORD>(pInfo->channels * 2);

    // xaudio stuff
    WAVEFORMATEX format = {
        .wFormatTag = WAVE_FORMAT_PCM,
        .nChannels = wChannels,
        .nSamplesPerSec = nSamplesPerSec,
        .nAvgBytesPerSec = nSamplesPerSec * nBlockAlign,
        .nBlockAlign = nBlockAlign,
        .wBitsPerSample = core::intCast<WORD>(2 * wChannels)
    };

    SoundBuffer buffer = {
        .name = std::string(file->getName()),
        .channels = core::intCast<size_t>(pInfo->channels),

        .format = format,
    };

    uint8_t data[0x1000];
    while (true) {
        int bitstream = 0;
        long ret = ov_read(&vf, (char*)data, sizeof(data), 0, 2, 1, &bitstream);
        if (ret == 0) {
            break; // done
        } else if (ret < 0) {
            LOG_WARN("failed to read ogg file: {} (error = {})", file->getName(), ret);
            break;
        }

        buffer.data.insert(buffer.data.end(), data, data + ret);
    }

    XAUDIO2_BUFFER xbuffer = {
        .Flags = XAUDIO2_END_OF_STREAM, // this is the only buffer
        .AudioBytes = core::intCast<UINT32>(buffer.data.size()),
        .pAudioData = static_cast<const BYTE*>(buffer.data.data())
    };

    buffer.buffer = xbuffer;

    ov_clear(&vf);

    return std::make_shared<SoundBuffer>(std::move(buffer));
}
