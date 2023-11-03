#include "engine/service/platform.h"
#include "engine/service/debug.h"
#include "engine/service/logging.h"

#include "engine/core/strings.h"

#include "engine/core/panic.h"

#include <intsafe.h> // DWORD_MAX
#include <comdef.h> // _com_error
#include <shellapi.h> // CommandLineToArgvW
#include <iostream>

using namespace simcoe;

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
}

bool PlatformService::createService() {
    ASSERTF(hInstance, "hInstance is not set, please call PlatformService::setup()");
    ASSERTF(nCmdShow != -1, "nCmdShow is not set, please call PlatformService::setup()");

    frequency = getClockFrequency();

    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        throwLastError("failed to set dpi awareness");
    }

    const WNDCLASSA kClass = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = Window::callback,
        .hInstance = hInstance,
        .lpszClassName = kClassName
    };

    if (RegisterClassA(&kClass) == 0) {
        throwLastError("failed to register window class");
    }

    char currentPath[0x1000];
    if (!GetModuleFileNameA(nullptr, currentPath, sizeof(currentPath))) {
        throwLastError("failed to get current path");
    }

    exeDirectory = fs::path(currentPath).parent_path();

    return true;
}

void PlatformService::destroyService() {
    UnregisterClassA(kClassName, hInstance);
}

void PlatformService::setup(HINSTANCE hInstance, int nCmdShow) {
    get()->hInstance = hInstance;
    get()->nCmdShow = nCmdShow;
}

CommandLine PlatformService::getCommandLine() {
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
    return PeekMessage(&get()->msg, NULL, 0, 0, PM_REMOVE) != 0;
}

bool PlatformService::waitForEvent() {
    return GetMessage(&get()->msg, NULL, 0, 0) != 0;
}

void PlatformService::dispatchEvent() {
    MSG msg = get()->msg;
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}

void PlatformService::quit(int code) {
    PostQuitMessage(code);
}

size_t PlatformService::getFrequency() {
    return get()->frequency;
}

size_t PlatformService::queryCounter() {
    return getClockCounter();
}

HINSTANCE PlatformService::getInstanceHandle() {
    return get()->hInstance;
}

int PlatformService::getShowCmd() {
    return get()->nCmdShow;
}

fs::path PlatformService::getExeDirectory() {
    return get()->exeDirectory;
}

void PlatformService::message(std::string_view title, std::string_view body) {
    MessageBox(nullptr, body.data(), title.data(), MB_ICONERROR | MB_SYSTEMMODAL);
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
        /* hInstance = */ PlatformService::getInstanceHandle(),
        /* lpParam = */ this
    );

    if (!hWindow) {
        throwLastError("failed to create window");
    }

    ShowWindow(hWindow, PlatformService::getShowCmd());
    UpdateWindow(hWindow);
}

Window::~Window() {
    if (hWindow != nullptr) {
        DestroyWindow(hWindow);
    }
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
