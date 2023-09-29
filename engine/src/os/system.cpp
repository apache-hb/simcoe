#include "engine/os/system.h"

#include <stdexcept>

using namespace simcoe;

namespace {
    constexpr const char *kClassName = "simcoe";

    constexpr DWORD getStyle(WindowStyle style) {
        switch (style) {
        case WindowStyle::eWindowed: return WS_OVERLAPPEDWINDOW;
        case WindowStyle::eBorderless: return WS_POPUP;
        default: throw std::runtime_error("invalid window style");
        }
    }
}

// window callback

LRESULT CALLBACK Window::callback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}

// window api

Window::Window(HINSTANCE hInstance, int nCmdShow, const WindowCreateInfo& createInfo) {
    hWindow = CreateWindow(
        /* lpClassName = */ kClassName,
        /* lpWindowName = */ createInfo.title,
        /* dwStyle = */ getStyle(createInfo.style),
        /* x = */ CW_USEDEFAULT,
        /* y = */ CW_USEDEFAULT,
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

Window System::createWindow(const WindowCreateInfo& createInfo) {
    return Window(hInstance, nCmdShow, createInfo);
}

bool System::getEvent() {
    return GetMessage(&msg, nullptr, 0, 0) != 0;
}

void System::dispatchEvent() {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}
