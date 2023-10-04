#pragma once

#include <windows.h>

#include "engine/math/math.h"

namespace simcoe {
    struct IWindowCallbacks {
        virtual ~IWindowCallbacks() = default;

        virtual void onResize(int width, int height) { }
        virtual void onClose() { }

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
        RECT getCoords() const;

        void enterFullscreen();
        void exitFullscreen();
        void moveWindow(RECT rect);

        static LRESULT CALLBACK callback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    private:
        void endResize(UINT width, UINT height);

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

        Window *createWindow(const WindowCreateInfo& createInfo);

        RECT nearestDisplayCoords(Window *pWindow);

        bool getEvent();
        void dispatchEvent();
        void quit();

    private:
        HINSTANCE hInstance;
        int nCmdShow;

        MSG msg;
    };
}
