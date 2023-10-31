#include "engine/service/platform.h"
#include "engine/service/debug.h"

#include "engine/util/strings.h"

#include <intsafe.h> // DWORD_MAX
#include <comdef.h> // _com_error
#include <dbghelp.h>

using namespace simcoe;

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

bool DebugService::createService() {
    if (!SymInitialize(GetCurrentProcess(), nullptr, TRUE)) {
        throwLastError("failed to initialize symbol engine");
    }

    return true;
}

void DebugService::destroyService() {
    SymCleanup(GetCurrentProcess());
}

std::vector<StackFrame> DebugService::backtrace() {
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


///
/// thread naming
/// the depths of windows engineers insanity knows no bounds
///

#pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO {
    DWORD dwType;
    LPCSTR szName;
    DWORD dwThreadID;
    DWORD dwFlags;
} THREADNAME_INFO;
#pragma pack(pop)

static constexpr DWORD dwRenameThreadMagic = 0x406D1388;

static void setThreadDesc(std::string_view name) {
    auto wide = util::widen(name);
    SetThreadDescription(GetCurrentThread(), wide.c_str());
}

void DebugService::setThreadName(std::string_view name) {
    // set name for PIX
    setThreadDesc(name);

    /// set debugger name
    THREADNAME_INFO info = {
        .dwType = 0x1000,
        .szName = name.data(),
        .dwThreadID = DWORD_MAX,
        .dwFlags = 0
    };

    __try {
        RaiseException(dwRenameThreadMagic, 0, sizeof(info) / sizeof(DWORD), reinterpret_cast<ULONG_PTR *>(&info));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

std::string DebugService::getThreadName() {
    PWSTR wide;
    if (HRESULT hr = GetThreadDescription(GetCurrentThread(), &wide); SUCCEEDED(hr)) {
        auto result = util::narrow(wide);
        LocalFree(wide);

        if (result.length() > 0) {
            return result;
        }
    }

    return std::format("0x{:x}", GetCurrentThreadId());
}

// error formatting

std::string DebugService::getResultName(HRESULT hr) {
    _com_error err(hr);
    return err.ErrorMessage();
}

bool endsWithAny(std::string_view str, std::string_view chars) {
    return str.find_last_of(chars) == str.size() - 1;
}

std::string DebugService::getErrorName(DWORD dwErrorCode) {
    char *pMessage = nullptr;
    DWORD err = FormatMessageA(
        /* dwFlags = */ FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        /* lpSource = */ nullptr,
        /* dwMessageId = */ dwErrorCode,
        /* dwLanguageId = */ MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        /* lpBuffer = */ reinterpret_cast<LPSTR>(&pMessage),
        /* nSize = */ 0,
        /* Arguments = */ nullptr
    );

    if (err == 0) {
        return std::format("0x{:x}", dwErrorCode);
    }

    std::string result = pMessage;
    LocalFree(pMessage);

    while (endsWithAny(result, "\n\r.")) {
        result.resize(result.size() - 1);
    }

    return result;
}

// utils

void simcoe::throwLastError(std::string_view msg, DWORD err) {
    throw std::runtime_error(std::format("{}: {}", msg, DebugService::getErrorName(err)));
}
