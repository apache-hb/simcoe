#include <windows.h>

#define DLLEXPORT __declspec(dllexport)

extern "C" {
    // load the agility sdk
    DLLEXPORT extern const UINT D3D12SDKVersion = 611;
    DLLEXPORT extern const char* D3D12SDKPath = ".\\editor.exe.p\\";
}

extern "C" {
    // ask vendors to use the high performance gpu if we have one
    DLLEXPORT extern const DWORD NvOptimusEnablement = 0x00000001;
    DLLEXPORT extern int AmdPowerXpressRequestHighPerformance = 1;
}
