#include "engine/os/system.h"

#include <stdexcept>

#include "engine/engine.h"

using namespace simcoe;

#define WM_USER_COMMAND (WM_USER + 1)
using UserCommandFn = void(*)(Window *pWindow);

namespace {
    constexpr const char *kClassName = "simcoe";

    constexpr DWORD getStyle(WindowStyle style) {
        switch (style) {
        case WindowStyle::eWindowed: return WS_OVERLAPPEDWINDOW;
        case WindowStyle::eBorderless: return WS_POPUP;
        default: throw std::runtime_error("invalid window style");
        }
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
        pWindow->pCallbacks->onClose();
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

Window::Window(HINSTANCE hInstance, int nCmdShow, const WindowCreateInfo& createInfo) : pCallbacks(createInfo.pCallbacks) {
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
        throw std::runtime_error("failed to create window");
    }

    ShowWindow(hWindow, nCmdShow);
    UpdateWindow(hWindow);
}

Window::~Window() {
    DestroyWindow(hWindow);
}

// callbacks

void Window::doResize(int width, int height, bool fullscreen) {
    logInfo("resize: {} {} {}", width, height, fullscreen);

    pCallbacks->onResize({
        .width = width,
        .height = height
    });
}

void Window::doSizeChange(WPARAM wParam, int width, int height) {
    if (bUserIsResizing) { return; }

    switch (wParam) {
    case SIZE_RESTORED:
        doResize(width, height, false);
        break;

    case SIZE_MAXIMIZED:
        doResize(width, height, true);
        break;

    default:
        break;
    }
}

void Window::beginUserResize() {
    bUserIsResizing = true;
}

void Window::endUserResize() {
    bUserIsResizing = false;
    RECT rect = getClientCoords();
    doResize(rect.right - rect.left, rect.bottom - rect.top, false);
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
    logInfo("enter fullscreen");
    sendCommand(this, [](Window *pWindow) {
        pWindow->bIgnoreNextResize = true;
    });
    ShowWindow(hWindow, SW_MAXIMIZE);
}

void Window::exitFullscreen() {
    logInfo("exit fullscreen");
    ShowWindow(hWindow, SW_RESTORE);
}

// system api

System::System(HINSTANCE hInstance, int nCmdShow)
    : hInstance(hInstance)
    , nCmdShow(nCmdShow)
{
    const WNDCLASSA cls = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = Window::callback,
        .hInstance = hInstance,
        .lpszClassName = kClassName
    };

    if (!RegisterClassA(&cls)) {
        throw std::runtime_error("failed to register window class");
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
    return (GetMessage(&msg, nullptr, 0, 0) != 0);
}

void System::dispatchEvent() {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}

void System::quit() {
    PostQuitMessage(0);
}
