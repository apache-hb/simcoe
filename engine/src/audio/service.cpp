#include "engine/audio/service.h"

#include "engine/threads/service.h"

#include "engine/log/service.h"

#include "engine/config/system.h"

#include "engine/core/error.h"

using namespace simcoe;

using namespace std::chrono_literals;

namespace {
    IXAudio2 *pAudioRuntime = nullptr;
    IXAudio2MasteringVoice *pAudioMasterVoice = nullptr;

    std::mutex gBufferMutex;
    std::vector<std::shared_ptr<audio::SoundBuffer>> gBuffers;

    std::mutex gVoiceMutex;
    std::vector<std::shared_ptr<audio::VoiceHandle>> gVoices;

    std::string xaErrorString(HRESULT hr) {
        switch (hr) {
        case XAUDIO2_E_INVALID_CALL: return "xaudio2:invalid-call";
        case XAUDIO2_E_XMA_DECODER_ERROR: return "xaudio2:xma-decoder-error";
        case XAUDIO2_E_XAPO_CREATION_FAILED: return "xaudio2:xapo-creation-failed";
        case XAUDIO2_E_DEVICE_INVALIDATED: return "xaudio2:device-invalidated";
        default: return debug::getResultName(hr);
        }
    }

    struct AudioCallbacks final : IXAudio2EngineCallback {
        void OnProcessingPassStart() noexcept override { }

        void OnProcessingPassEnd() noexcept override { }

        void OnCriticalError(HRESULT hr) noexcept override {
            core::throwNonFatal("xaudio2 critical error: {}", xaErrorString(hr));
        }
    };

    AudioCallbacks gAudioCallbacks;
}

#define XA_CHECK(EXPR) \
    do { \
        if (HRESULT hr = (EXPR); FAILED(hr)) { \
            core::throwNonFatal("xaudio2 error: {}", xaErrorString(hr)); \
        } \
    } while (false)

bool AudioService::createService() {
    HR_CHECK(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

#if SM_DEBUG_AUDIO
    UINT32 flags = XAUDIO2_DEBUG_ENGINE;
#else
    UINT32 flags = 0;
#endif

    XA_CHECK(XAudio2Create(&pAudioRuntime, flags, XAUDIO2_USE_DEFAULT_PROCESSOR));
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

std::shared_ptr<audio::SoundHandle> AudioService::playSound(std::shared_ptr<audio::SoundBuffer> buffer) {
    IXAudio2SourceVoice *pVoice = nullptr;
    XA_CHECK(pAudioRuntime->CreateSourceVoice(&pVoice, (WAVEFORMATEX*)&buffer->format));

    XA_CHECK(pVoice->SubmitSourceBuffer(&buffer->buffer));

    XA_CHECK(pVoice->Start());

    return std::make_shared<audio::SoundHandle>(pVoice);
}
