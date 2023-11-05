#include "engine/service/platform.h"
#include "engine/service/debug.h"

#include "engine/core/strings.h"
#include "engine/core/panic.h"

#include "engine/config/ext/builder.h"

#include "engine/log/service.h"

#include <intsafe.h> // DWORD_MAX
#include <comdef.h> // _com_error
#include <shellapi.h> // CommandLineToArgvW
#include <iostream>

using namespace simcoe;

// platform

namespace {
    constexpr auto kClassName = "simcoe";

    namespace cfg {
        std::string windowTitle = "simcoe";
        WindowSize windowSize = { 1280, 720 };
    }

    HINSTANCE gInstance = nullptr;
    int gCmdShow = -1;
    IWindowCallbacks *gCallbacks = nullptr;

    fs::path exeDirectory;
    size_t gFrequency = 0;

    Window *gWindow = nullptr;
    MSG gMsg = {};

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
}

PlatformService::PlatformService() {
    CFG_DECLARE("platform",
        CFG_FIELD_TABLE("window",
            CFG_FIELD_STRING("title", &cfg::windowTitle),
            CFG_FIELD_TABLE("size",
                CFG_FIELD_INT("width", &cfg::windowSize.width),
                CFG_FIELD_INT("height", &cfg::windowSize.height)
            )
        )
    );
}

bool PlatformService::createService() {
    ASSERTF(gInstance, "hInstance is not set, please call PlatformService::setup()");
    ASSERTF(gCmdShow != -1, "nCmdShow is not set, please call PlatformService::setup()");
    ASSERTF(gCallbacks != nullptr, "window callbacks are not set, please call PlatformService::setup()");

    gFrequency = getClockFrequency();

    LOG_INFO("frequency: {} Hz", gFrequency);

    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        debug::throwLastError("failed to set dpi awareness");
    }

    const WNDCLASSA kClass = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = Window::callback,
        .hInstance = gInstance,
        .lpszClassName = kClassName
    };

    if (RegisterClassA(&kClass) == 0) {
        debug::throwLastError("failed to register window class");
    }

    char currentPath[0x1000];
    if (!GetModuleFileNameA(nullptr, currentPath, sizeof(currentPath))) {
        debug::throwLastError("failed to get current path");
    }

    exeDirectory = fs::path(currentPath).parent_path();

    WindowCreateInfo info = {
        .title = cfg::windowTitle.c_str(),
        .style = WindowStyle::eWindowed,
        .size = cfg::windowSize,
        .pCallbacks = gCallbacks
    };
    gWindow = new Window(info);

    return true;
}

void PlatformService::destroyService() {
    UnregisterClassA(kClassName, gInstance);
}

void PlatformService::setup(HINSTANCE hInstance, int nCmdShow, IWindowCallbacks *pCallbacks) {
    gInstance = hInstance;
    gCmdShow = nCmdShow;
    gCallbacks = pCallbacks;
}

CommandLine system::getCommandLine() {
    CommandLine args;

    int argc;
    auto **argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    for (int i = 0; i < argc; i++) {
        args.push_back(util::narrow(argv[i]));
    }

    LocalFree(argv);
    return args;
}

bool PlatformService::getEvent() {
    return PeekMessage(&gMsg, NULL, 0, 0, PM_REMOVE) != 0;
}

bool PlatformService::waitForEvent() {
    return GetMessage(&gMsg, NULL, 0, 0) != 0;
}

void PlatformService::dispatchEvent() {
    TranslateMessage(&gMsg);
    DispatchMessage(&gMsg);
}

void PlatformService::quit(int code) {
    PostQuitMessage(code);
}

size_t PlatformService::getFrequency() {
    return gFrequency;
}

size_t PlatformService::queryCounter() {
    return getClockCounter();
}

Window& PlatformService::getWindow() {
    return *gWindow;
}

void PlatformService::showWindow() {
    getWindow().showWindow();
}

const fs::path& PlatformService::getExeDirectory() {
    return exeDirectory;
}

void PlatformService::message(std::string_view title, std::string_view body) {
    HWND hWnd = gWindow == nullptr ? nullptr : gWindow->getHandle();
    MessageBox(hWnd, body.data(), title.data(), MB_ICONERROR | MB_SYSTEMMODAL);
    std::cout << title << ": " << body << std::endl;
}

// clock

Clock::Clock()
    : start(PlatformService::queryCounter())
{ }

float Clock::now() const {
    size_t counter = PlatformService::queryCounter();
    return float(counter - start) / PlatformService::getFrequency();
}

uint32_t Clock::ms() const {
    size_t counter = PlatformService::queryCounter();
    return uint32_t((counter - start) * 1000 / PlatformService::getFrequency());
}

// window callback

#define WM_USER_COMMAND (WM_USER + 1)
using UserCommandFn = void(*)(Window *pWindow);

LRESULT CALLBACK Window::callback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Window *pWindow = reinterpret_cast<Window *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

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

namespace {
    constexpr DWORD getStyle(WindowStyle style) {
        switch (style) {
        case WindowStyle::eWindowed: return WS_OVERLAPPEDWINDOW;
        case WindowStyle::eBorderlessFixed: return WS_POPUP;
        case WindowStyle::eBorderlessMoveable: return WS_POPUP | WS_THICKFRAME;
        default: throw std::runtime_error("invalid window style");
        }
    }

    void sendCommand(Window *pWindow, UserCommandFn fn) {
        PostMessage(pWindow->getHandle(), WM_USER_COMMAND, reinterpret_cast<WPARAM>(fn), 0);
    }
}

Window::Window(const WindowCreateInfo& createInfo)
    : pCallbacks(createInfo.pCallbacks)
{
    ASSERTF(pCallbacks != nullptr, "window callbacks must be provided");
    auto [width, height] = createInfo.size;

    hWindow = CreateWindow(
        /* lpClassName = */ kClassName,
        /* lpWindowName = */ createInfo.title,
        /* dwStyle = */ getStyle(createInfo.style),
        /* x = */ (GetSystemMetrics(SM_CXSCREEN) - width) / 2,
        /* y = */ (GetSystemMetrics(SM_CYSCREEN) - height) / 2,
        /* nWidth = */ width,
        /* nHeight = */ height,
        /* hWndParent = */ nullptr,
        /* hMenu = */ nullptr,
        /* hInstance = */ gInstance,
        /* lpParam = */ this
    );

    if (!hWindow) {
        debug::throwLastError("failed to create window");
    }

    showWindow();
}

Window::~Window() {
    if (hWindow != nullptr) {
        DestroyWindow(hWindow);
    }
}

void Window::showWindow() {
    ShowWindow(hWindow, gCmdShow);
    UpdateWindow(hWindow);
}

// callbacks

void Window::doResize(int width, int height) {
    pCallbacks->onResize(WindowSize::from(width, height));
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

WindowSize Window::getSize() const {
    RECT rect;
    GetClientRect(hWindow, &rect);
    return WindowSize::from(rect.right - rect.left, rect.bottom - rect.top);
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
