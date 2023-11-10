#include "engine/audio/service.h"

#include "engine/log/message.h"
#include "engine/threads/service.h"
#include "engine/threads/mutex.h"

#include "engine/log/service.h"

#include "engine/config/system.h"

#include "engine/core/error.h"

using namespace simcoe;

using namespace std::chrono_literals;

namespace {
    IXAudio2 *pAudioRuntime = nullptr;
    IXAudio2MasteringVoice *pAudioMasterVoice = nullptr;

    mt::SharedMutex gBufferMutex{"buffers"};
    std::vector<audio::SoundBufferPtr> gBuffers;

    mt::SharedMutex gVoiceMutex{"voices"};
    std::vector<audio::VoiceHandlePtr> gVoices;

    struct AudioCallbacks final : IXAudio2EngineCallback {
        void OnProcessingPassStart() noexcept override { }

        void OnProcessingPassEnd() noexcept override { }

        void OnCriticalError(HRESULT hr) noexcept override {
            LOG_ERROR("xaudio2 critical error: {}", audio::xaErrorString(hr));
        }
    };

    AudioCallbacks gAudioCallbacks;
}

bool AudioService::createService() {
    HR_CHECK(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

#if SM_DEBUG_AUDIO
    UINT32 xaFlags = XAUDIO2_DEBUG_ENGINE;
#else
    UINT32 xaFlags = 0;
#endif

    XA_CHECK(XAudio2Create(&pAudioRuntime, xaFlags, XAUDIO2_USE_DEFAULT_PROCESSOR));
    XA_CHECK(pAudioRuntime->CreateMasteringVoice(&pAudioMasterVoice));

#if SM_DEBUG_AUDIO
    const XAUDIO2_DEBUG_CONFIGURATION kDebugConfig = {
        .TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS,
        .BreakMask = debug::isAttached() ? XAUDIO2_LOG_ERRORS : 0u,
        .LogThreadID = true,
        .LogFileline = true,
        .LogFunctionName = true,
        .LogTiming = true
    };
    pAudioRuntime->SetDebugConfiguration(&kDebugConfig);
    LOG_INFO("Audio debug logging enabled");
#endif

    pAudioRuntime->RegisterForCallbacks(&gAudioCallbacks);

    XA_CHECK(pAudioRuntime->StartEngine());

    return true;
}

void AudioService::destroyService() {
    mt::WriteLock voiceLock(gVoiceMutex);
    mt::WriteLock bufferLock(gBufferMutex);

    gVoices.clear();
    gBuffers.clear();

    pAudioRuntime->StopEngine();

    if (pAudioMasterVoice) {
        pAudioMasterVoice->DestroyVoice();
        pAudioMasterVoice = nullptr;
    }

    if (pAudioRuntime) {
        ULONG refs = pAudioRuntime->Release();
        pAudioRuntime = nullptr;

        SM_ASSERTF(refs == 0, "XAudio2 runtime was released with {} references left", refs);
    }

    CoUninitialize();
}

mt::SharedMutex& AudioService::getBufferMutex() { return gBufferMutex; }
std::vector<audio::SoundBufferPtr>& AudioService::getBuffers() { return gBuffers; }

mt::SharedMutex& AudioService::getVoiceMutex() { return gVoiceMutex; }
std::vector<audio::VoiceHandlePtr>& AudioService::getVoices() { return gVoices; }

audio::SoundBufferPtr AudioService::loadVorbisOgg(std::shared_ptr<depot::IFile> buffer) {
    auto sound = audio::loadVorbisOgg(buffer); // TODO: keep data in service memory

    mt::WriteLock lock(gBufferMutex);
    gBuffers.push_back(sound);
    return sound;
}

audio::VoiceHandlePtr AudioService::createVoice(std::string name, const audio::SoundFormat& format) {
    log::PendingMessage msg{"AudioService::createVoice"};
    msg.addLine("name: {}", name);
    msg.addLine("tag: {}", WORD(format.getFormatTag()));
    msg.addLine("channels: {}", format.getChannels());
    msg.addLine("samples/sec: {}", format.getSamplesPerSecond());
    msg.addLine("bits/sample: {}", format.getBitsPerSample());
    msg.addLine("block align: {}", WORD(format.getFormat()->nBlockAlign));
    msg.addLine("avg bytes/sec: {}", DWORD(format.getFormat()->nAvgBytesPerSec));
    msg.send(log::eDebug);

    IXAudio2SourceVoice *pVoice = nullptr;
    XA_CHECK(pAudioRuntime->CreateSourceVoice(&pVoice, format.getFormat()));

    auto voice = std::make_shared<audio::VoiceHandle>(name, format, pVoice);
    mt::WriteLock lock(gVoiceMutex);
    gVoices.push_back(voice);
    return voice;
}
