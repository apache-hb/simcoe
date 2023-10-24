#include "engine/system/system.h"
#include "engine/util/strings.h"

#include <intsafe.h>
#include <comdef.h>
#include <dbghelp.h>
#include <stdexcept>

using namespace simcoe;
using namespace simcoe::system;

#define WM_USER_COMMAND (WM_USER + 1)
using UserCommandFn = void(*)(Window *pWindow);

namespace {
    // window
    constexpr const char *kClassName = "simcoe";

    constexpr DWORD getStyle(WindowStyle style) {
        switch (style) {
        case WindowStyle::eWindowed: return WS_OVERLAPPEDWINDOW;
        case WindowStyle::eBorderlessFixed: return WS_POPUP;
        case WindowStyle::eBorderlessMoveable: return WS_POPUP | WS_THICKFRAME;
        default: throw std::runtime_error("invalid window style");
        }
    }

    void throwLastError(const char *msg) {
        throw std::runtime_error(std::format("{}: {}", msg, getWin32ErrorName(GetLastError())));
    }

    Window *getWindow(HWND hWnd) {
        return reinterpret_cast<Window *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    RECT getNearestMonitorCoords(HWND hWindow) {
        RECT rect;
        GetWindowRect(hWindow, &rect);

        POINT pt = { rect.left, rect.top };
        HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);

        MONITORINFO info = { sizeof(MONITORINFO) };
        GetMonitorInfoA(hMonitor, &info);

        return info.rcMonitor;
    }

    void sendCommand(Window *pWindow, UserCommandFn fn) {
        PostMessage(pWindow->getHandle(), WM_USER_COMMAND, reinterpret_cast<WPARAM>(fn), 0);
    }

    // clock
    size_t getFrequency() {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        return frequency.QuadPart;
    }

    size_t getCounter() {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return counter.QuadPart;
    }

    size_t gFrequency;
}

// window callback

LRESULT CALLBACK Window::callback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Window *pWindow = getWindow(hWnd);

    switch (uMsg) {
    case WM_CREATE: {
        CREATESTRUCT *pCreateStruct = reinterpret_cast<CREATESTRUCT *>(lParam);
        pWindow = reinterpret_cast<Window *>(pCreateStruct->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow));
        return 0;
    }

    case WM_CLOSE:
        pWindow->closeWindow();
        return 0;

    case WM_ENTERSIZEMOVE:
        pWindow->beginUserResize();
        break;

    case WM_EXITSIZEMOVE:
        pWindow->endUserResize();
        return 0;

    case WM_SIZE:
        pWindow->doSizeChange(wParam, LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_USER_COMMAND:
        reinterpret_cast<UserCommandFn>(wParam)(pWindow);
        return 0;

    default:
        break;
    }

    if (pWindow != nullptr && pWindow->pCallbacks->onEvent(hWnd, uMsg, wParam, lParam)) {
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// window api

Window::Window(HINSTANCE hInstance, int nCmdShow, const WindowCreateInfo& createInfo)
    : pCallbacks(createInfo.pCallbacks)
{
    ASSERTF(pCallbacks != nullptr, "window callbacks must be provided");

    hWindow = CreateWindow(
        /* lpClassName = */ kClassName,
        /* lpWindowName = */ createInfo.title,
        /* dwStyle = */ getStyle(createInfo.style),
        /* x = */ (GetSystemMetrics(SM_CXSCREEN) - createInfo.width) / 2,
        /* y = */ (GetSystemMetrics(SM_CYSCREEN) - createInfo.height) / 2,
        /* nWidth = */ createInfo.width,
        /* nHeight = */ createInfo.height,
        /* hWndParent = */ nullptr,
        /* hMenu = */ nullptr,
        /* hInstance = */ hInstance,
        /* lpParam = */ this
    );

    if (!hWindow) {
        throwLastError("failed to create window");
    }

    ShowWindow(hWindow, nCmdShow);
    UpdateWindow(hWindow);
}

Window::~Window() {
    if (hWindow != nullptr) {
        DestroyWindow(hWindow);
    }
}

// callbacks

void Window::doResize(int width, int height) {
    pCallbacks->onResize(ResizeEvent::from(width, height));
}

void Window::doSizeChange(WPARAM wParam, int width, int height) {
    if (bUserIsResizing) { return; }

    switch (wParam) {
    case SIZE_RESTORED:
        doResize(width, height);
        break;

    case SIZE_MAXIMIZED:
        doResize(width, height);
        break;

    default:
        break;
    }
}

void Window::closeWindow() {
    pCallbacks->onClose();
    DestroyWindow(hWindow);
    hWindow = nullptr;
}

void Window::beginUserResize() {
    bUserIsResizing = true;
}

void Window::endUserResize() {
    bUserIsResizing = false;
    RECT rect = getClientCoords();
    doResize(rect.right - rect.left, rect.bottom - rect.top);
}

// window getters

HWND Window::getHandle() const {
    return hWindow;
}

math::int2 Window::getSize() const {
    RECT rect;
    GetClientRect(hWindow, &rect);
    return math::int2{rect.right - rect.left, rect.bottom - rect.top};
}

RECT Window::getWindowCoords() const {
    RECT rect;
    GetWindowRect(hWindow, &rect);
    return rect;
}

RECT Window::getClientCoords() const {
    RECT rect;
    GetClientRect(hWindow, &rect);
    return rect;
}

void Window::enterFullscreen() {
    sendCommand(this, [](Window *pWindow) {
        pWindow->bIgnoreNextResize = true;
    });
    ShowWindow(hWindow, SW_MAXIMIZE);
}

void Window::exitFullscreen() {
    ShowWindow(hWindow, SW_RESTORE);
}

void Window::setStyle(WindowStyle style) {
    SetWindowLongPtr(hWindow, GWL_STYLE, getStyle(style));
}

// system api

System::System(HINSTANCE hInstance, int nCmdShow)
    : hInstance(hInstance)
    , nCmdShow(nCmdShow)
{
    if (!SymInitialize(GetCurrentProcess(), NULL, TRUE)) {
        throwLastError("failed to initialize debug symbols");
    }

    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        throwLastError("failed to set dpi awareness");
    }

    gFrequency = getFrequency();

    const WNDCLASSA cls = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = Window::callback,
        .hInstance = hInstance,
        .lpszClassName = kClassName
    };

    if (RegisterClassA(&cls) == 0) {
        throwLastError("failed to register window class");
    }
}

System::~System() {
    UnregisterClassA(kClassName, hInstance);
}

Window *System::createWindow(const WindowCreateInfo& createInfo) {
    return new Window(hInstance, nCmdShow, createInfo);
}

RECT System::nearestDisplayCoords(Window *pWindow) {
    return getNearestMonitorCoords(pWindow->getHandle());
}

bool System::getEvent() {
    return GetMessage(&msg, NULL, 0, 0) != 0;
}

void System::dispatchEvent() {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}

void System::quit() {
    PostQuitMessage(0);
}

/// clock api

Clock::Clock()
    : start(getCounter())
{ }

float Clock::now() const {
    size_t counter = getCounter();
    return float(counter - start) / gFrequency;
}

/// error reporting

std::string system::getErrorName(HRESULT hr) {
    _com_error err(hr);
    return std::format("{} (0x{:x})", err.ErrorMessage(), unsigned(hr));
}

std::string system::getWin32ErrorName(DWORD dwErrorCode) {
    char *pMessage = nullptr;
    FormatMessageA(
        /* dwFlags = */ FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        /* lpSource = */ nullptr,
        /* dwMessageId = */ dwErrorCode,
        /* dwLanguageId = */ MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        /* lpBuffer = */ reinterpret_cast<LPSTR>(&pMessage),
        /* nSize = */ 0,
        /* Arguments = */ nullptr
    );

    std::string result = pMessage;
    LocalFree(pMessage);
    return result;
}

///
/// backtrace
///

static constexpr size_t kNameLength = 512;

static BOOL getFrame(STACKFRAME *frame, CONTEXT *ctx, HANDLE process, HANDLE thread) {
    return StackWalk(
        /* MachineType = */ IMAGE_FILE_MACHINE_AMD64,
        /* hProcess = */ process,
        /* kThread = */ thread,
        /* StackFrame = */ frame,
        /* Context = */ ctx,
        /* ReadMemoryRoutine = */ NULL,
        /* FunctionTableAccessRoutine = */ SymFunctionTableAccess64,
        /* GetModuleBaseRoutine = */ SymGetModuleBase64,
        /* TranslateAddress = */ NULL
    );
}

std::vector<std::string> system::getBacktrace() {
    HANDLE thread = GetCurrentThread();
    HANDLE process = GetCurrentProcess();

    IMAGEHLP_SYMBOL *symbol = (IMAGEHLP_SYMBOL*)malloc(sizeof(IMAGEHLP_SYMBOL) + kNameLength);
    symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
    symbol->Address = 0;
    symbol->Size = 0;
    symbol->Flags = SYMF_FUNCTION;
    symbol->MaxNameLength = kNameLength;

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

    std::vector<std::string> result;
    while (getFrame(&frame, &ctx, process, thread)) {
        char name[kNameLength] = { 0 };

        SymGetSymFromAddr(process, frame.AddrPC.Offset, &disp, symbol);
        UnDecorateSymbolName(symbol->Name, name, kNameLength, UNDNAME_COMPLETE);

        result.push_back(std::format("{} (0x{:x})", name, frame.AddrPC.Offset));
    }

    free(symbol);

    return result;
}

void system::printBacktrace() {
    auto backtrace = getBacktrace();
    for (auto &line : backtrace) {
        simcoe::logInfo("{}", line);
    }
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

static void setThreadDesc(const char *name) {
    auto wide = util::widen(name);
    SetThreadDescription(GetCurrentThread(), wide.c_str());
}

void system::setThreadName(const char *name) {
    // set name for PIX
    setThreadDesc(name);

    /// set debugger name
    THREADNAME_INFO info = {
        .dwType = 0x1000,
        .szName = name,
        .dwThreadID = DWORD_MAX,
        .dwFlags = 0
    };

    __try {
        RaiseException(dwRenameThreadMagic, 0, sizeof(info) / sizeof(DWORD), reinterpret_cast<ULONG_PTR *>(&info));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

std::string system::getThreadName() {
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
