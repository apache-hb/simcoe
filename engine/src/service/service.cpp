#include "engine/service/service.h"
#include "engine/service/platform.h"
#include "engine/service/debug.h"
#include "engine/service/truetype.h"

#include "engine/system/system.h"

#include "engine/engine.h"

#include <intsafe.h>
#include <comdef.h>
#include <dbghelp.h>

using namespace simcoe;

// service runtime

ServiceRuntime::ServiceRuntime(std::span<IService*> services)
    : services(services)
{
    for (IService *service : services) {
        service->create();
    }
}

ServiceRuntime::~ServiceRuntime() {
    for (IService *service : services) {
        service->destroy();
    }
}

// platform

namespace {
    constexpr auto kClassName = "simcoe";

    size_t getClockFrequency() {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        return frequency.QuadPart;
    }

    size_t getClockCounter() {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return counter.QuadPart;
    }

    void throwLastError(const char *msg) {
        throw std::runtime_error(std::format("{}: {}", msg, system::getWin32ErrorName(GetLastError())));
    }

    LRESULT CALLBACK windowCallback(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        return DefWindowProcA(hWindow, uMsg, wParam, lParam);
    }
}

void PlatformService::createService() {
    ASSERTF(hInstance, "hInstance is not set, please call PlatformService::setup()");
    ASSERTF(nCmdShow != -1, "nCmdShow is not set, please call PlatformService::setup()");

    frequency = getClockFrequency();

    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        throwLastError("failed to set dpi awareness");
    }

    const WNDCLASSA kClass = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = windowCallback,
        .hInstance = hInstance,
        .lpszClassName = kClassName
    };

    if (RegisterClassA(&kClass) == 0) {
        throwLastError("failed to register window class");
    }
}

void PlatformService::destroyService() {
    UnregisterClassA(kClassName, hInstance);
}

void PlatformService::setup(HINSTANCE hInstance, int nCmdShow) {
    get()->hInstance = hInstance;
    get()->nCmdShow = nCmdShow;
}

// debug

namespace {
    constexpr size_t kNameLength = 0x1000;

    BOOL getFrame(STACKFRAME *pFrame, CONTEXT *pContext, HANDLE hProcess, HANDLE hThread) {
        return StackWalk(
            /* MachineType = */ IMAGE_FILE_MACHINE_AMD64,
            /* hProcess = */ hProcess,
            /* kThread = */ hThread,
            /* StackFrame = */ pFrame,
            /* Context = */ pContext,
            /* ReadMemoryRoutine = */ NULL,
            /* FunctionTableAccessRoutine = */ SymFunctionTableAccess64,
            /* GetModuleBaseRoutine = */ SymGetModuleBase64,
            /* TranslateAddress = */ NULL
        );
    }
}

void DebugService::createService() {
    if (!SymInitialize(GetCurrentProcess(), nullptr, TRUE)) {
        throwLastError("failed to initialize symbol engine");
    }
}

void DebugService::destroyService() {
    SymCleanup(GetCurrentProcess());
}


std::vector<StackFrame> DebugService::getBacktrace() {
    HANDLE hThread = GetCurrentThread();
    HANDLE hProcess = GetCurrentProcess();

    std::unique_ptr<IMAGEHLP_SYMBOL, decltype(&free)> pSymbol{(IMAGEHLP_SYMBOL*)malloc(sizeof(IMAGEHLP_SYMBOL) + kNameLength), free};
    pSymbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
    pSymbol->Address = 0;
    pSymbol->Size = 0;
    pSymbol->Flags = SYMF_FUNCTION;
    pSymbol->MaxNameLength = kNameLength;

    DWORD64 disp = 0;

    CONTEXT ctx = { 0 };
    RtlCaptureContext(&ctx);

    STACKFRAME frame = {
        .AddrPC = {
            .Offset = ctx.Rip,
            .Mode = AddrModeFlat
        },
        .AddrFrame = {
            .Offset = ctx.Rbp,
            .Mode = AddrModeFlat
        },
        .AddrStack = {
            .Offset = ctx.Rsp,
            .Mode = AddrModeFlat
        }
    };

    std::vector<StackFrame> result;
    char name[kNameLength] = {};

    while (getFrame(&frame, &ctx, hProcess, hThread)) {
        name[0] = '\0';
        SymGetSymFromAddr(hProcess, frame.AddrPC.Offset, &disp, pSymbol.get());
        if (UnDecorateSymbolName(pSymbol->Name, name, kNameLength, UNDNAME_COMPLETE)) {
            result.push_back({ name, frame.AddrPC.Offset });
        } else {
            result.push_back({ pSymbol->Name, frame.AddrPC.Offset });
        }
    }

    return result;
}

// truetype

void TrueTypeService::createService() {
    if (FT_Error error = FT_Init_FreeType(&library); error) {
        logAssert("failed to initialize FreeType library (fterr={})", FT_Error_String(error));
    }
}

void TrueTypeService::destroyService() {
    FT_Done_FreeType(library);
}
