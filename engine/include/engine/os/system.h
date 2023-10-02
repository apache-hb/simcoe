#pragma once

#include <windows.h>

#include "engine/math/math.h"

namespace simcoe {
    struct IWindowCallbacks {
        virtual ~IWindowCallbacks() = default;

        virtual void onResize(int width, int height) { }

        virtual bool onEvent(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { return false; }
    };

    enum class WindowStyle {
        eWindowed,
        eBorderless
    };

    struct WindowCreateInfo {
        const char *title;
        WindowStyle style;
        int width;
        int height;

        IWindowCallbacks *pCallbacks;
    };

    struct Window {
        Window(HINSTANCE hInstance, int nCmdShow, const WindowCreateInfo& createInfo);
        ~Window();

        HWND getHandle() const;
        math::int2 getSize() const;

        static LRESULT CALLBACK callback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    private:
        HWND hWindow;
        IWindowCallbacks *pCallbacks;
    };

    struct Timer {
        Timer();

        float now();

    private:
        size_t frequency;
        size_t start;
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
