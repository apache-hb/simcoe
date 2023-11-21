#include "engine/audio/audio.h"

#include "engine/core/units.h"
#include "engine/core/error.h"

#include "engine/debug/service.h"
#include "engine/log/service.h"

using namespace simcoe;
using namespace simcoe::audio;

std::string audio::xaErrorString(HRESULT hr) {
    switch (hr) {
    case XAUDIO2_E_INVALID_CALL: return "xaudio2:invalid-call";
    case XAUDIO2_E_XMA_DECODER_ERROR: return "xaudio2:xma-decoder-error";
    case XAUDIO2_E_XAPO_CREATION_FAILED: return "xaudio2:xapo-creation-failed";
    case XAUDIO2_E_DEVICE_INVALIDATED: return "xaudio2:device-invalidated";
    default: return debug::getResultName(hr);
    }
}

SoundBuffer::SoundBuffer(std::string name, std::vector<uint8_t> bytes, SoundFormat format)
    : name(std::move(name))
    , channels(format.getChannels())
    , data(std::move(bytes))
    , format(format)
{
    buffer.AudioBytes = core::intCast<UINT32>(data.size());
    buffer.pAudioData = static_cast<const BYTE*>(data.data());
    buffer.Flags = XAUDIO2_END_OF_STREAM;
}

SoundBuffer::~SoundBuffer() {

}

VoiceHandle::~VoiceHandle() {
    pVoice->DestroyVoice();
}

void VoiceHandle::submit(audio::SoundBufferPtr buffer) {
    LOG_DEBUG("submitting buffer `{}` to `{}`", buffer->getName(), name);
    XA_CHECK(pVoice->SubmitSourceBuffer(buffer->getBuffer())); // TODO: track lifetimes properly
    resume();
}

void VoiceHandle::pause() {
    XA_CHECK(pVoice->Stop());
}

void VoiceHandle::resume() {
    XA_CHECK(pVoice->Start());
}

void VoiceHandle::reset() {
    XA_CHECK(pVoice->FlushSourceBuffers());
}

float VoiceHandle::getVolume() const {
    float volume = 0.0f;
    pVoice->GetVolume(&volume);
    return volume;
}

void VoiceHandle::setVolume(float volume) {
    XA_CHECK(pVoice->SetVolume(volume));
}

bool VoiceHandle::isPlaying() const {
    XAUDIO2_VOICE_STATE state;
    pVoice->GetState(&state);
    return state.BuffersQueued > 0;
}
