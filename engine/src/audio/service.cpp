#include "engine/audio/service.h"

#include "engine/core/error.h"

#include <xaudio2.h>

using namespace simcoe;

namespace {
    IXAudio2 *pAudioRuntime = nullptr;
    IXAudio2MasteringVoice *pAudioMasterVoice = nullptr;
}

bool AudioService::createService() {
    HR_CHECK(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    HR_CHECK(XAudio2Create(&pAudioRuntime, 0, XAUDIO2_DEFAULT_PROCESSOR));
    HR_CHECK(pAudioRuntime->CreateMasteringVoice(&pAudioMasterVoice));

    return true;
}

void AudioService::destroyService() {
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
