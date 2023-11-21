#include "engine/debug/service.h"
#include "engine/core/error.h"
#include "engine/log/service.h"

#include "engine/core/unique.h"
#include "engine/core/strings.h"

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
        debug::throwLastError("failed to initialize symbol engine");
    }

    return true;
}

void DebugService::destroyService() {
    SymCleanup(GetCurrentProcess());
}

debug::Backtrace DebugService::backtrace() {
    HANDLE hThread = GetCurrentThread();
    HANDLE hProcess = GetCurrentProcess();

    std::unique_ptr<IMAGEHLP_SYMBOL, decltype(&free)> pSymbol{(IMAGEHLP_SYMBOL*)malloc(sizeof(IMAGEHLP_SYMBOL) + kNameLength), free};
    pSymbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
    pSymbol->Address = 0;
    pSymbol->Size = 0;
    pSymbol->Flags = SYMF_FUNCTION;
    pSymbol->MaxNameLength = kNameLength;

    DWORD64 disp = 0;

    CONTEXT ctx = { };
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

    debug::Backtrace result;
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

namespace {
    constexpr DWORD kRenameThreadMagic = 0x406D1388;

    void setThreadDebugName(std::string_view name) {
        auto wide = util::widen(name);
        if (HRESULT hr = SetThreadDescription(GetCurrentThread(), wide.c_str()); FAILED(hr)) {
            LOG_WARN("failed to set thread name `{}` (hr = {})", name, debug::getResultName(hr));
        }
    }
}

void debug::setThreadName(std::string_view name) {
    setThreadDebugName(name);

    /// other debuggers use this to name threads
    const THREADNAME_INFO info = {
        .dwType = 0x1000,
        .szName = name.data(),
        .dwThreadID = DWORD_MAX,
        .dwFlags = 0
    };

    __try {
        RaiseException(kRenameThreadMagic, 0, sizeof(THREADNAME_INFO) / sizeof(DWORD), reinterpret_cast<const ULONG_PTR *>(&info));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// error formatting

std::string debug::getResultName(HRESULT hr) {
    _com_error err(hr);
    return err.ErrorMessage();
}

bool endsWithAny(std::string_view str, std::string_view chars) {
    return str.find_last_of(chars) == str.size() - 1;
}

std::string debug::getErrorName(DWORD dwErrorCode) {
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
        return fmt::format("0x{:x}", dwErrorCode);
    }

    std::string result = pMessage;
    LocalFree(pMessage);

    while (endsWithAny(result, "\n\r.")) {
        result.resize(result.size() - 1);
    }

    return result;
}

// utils

void debug::throwLastError(std::string_view msg, DWORD err) {
    core::throwFatal("{} ({})", msg, getErrorName(err));
}

bool debug::isAttached() {
    return IsDebuggerPresent();
}
