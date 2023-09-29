#pragma once

#include <windows.h>

namespace simcoe {
    enum class WindowStyle {
        eWindowed,
        eBorderless
    };

    struct WindowCreateInfo {
        const char *title;
        WindowStyle style;
        int width;
        int height;
    };

    struct Window {
        Window(HINSTANCE hInstance, int nCmdShow, const WindowCreateInfo& createInfo);
        ~Window();

        HWND getHandle() const { return hWindow; }

        static LRESULT CALLBACK callback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    private:
        HWND hWindow;
    };

    struct System {
        System(HINSTANCE hInstance, int nCmdShow);
        ~System();

        Window createWindow(const WindowCreateInfo& createInfo);

        bool getEvent();
        void dispatchEvent();

    private:
        HINSTANCE hInstance;
        int nCmdShow;

        MSG msg;
    };
}
