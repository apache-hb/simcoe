#pragma once

#include <atomic>
#include <windows.h>

#include "engine/math/math.h"

namespace simcoe {
    struct ResizeEvent {
        int width;
        int height;
    };

    struct IWindowCallbacks {
        virtual ~IWindowCallbacks() = default;

        virtual void onResize(const ResizeEvent& event) { }
        virtual void onFullscreen(bool bFullscreen) { }
        virtual void onClose() { }

        virtual bool onEvent(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { return false; }
    };

    enum class WindowStyle {
        eWindowed,
        eBorderlessMoveable,
        eBorderlessFixed
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
        RECT getWindowCoords() const;
        RECT getClientCoords() const;

        void enterFullscreen();
        void exitFullscreen();

        void setStyle(WindowStyle style);

        static LRESULT CALLBACK callback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    private:
        void doResize(int width, int height, bool fullscreen);
        void doSizeChange(WPARAM wParam, int width, int height);

        void beginUserResize();
        void endUserResize();

        bool bUserIsResizing = false;
        bool bIgnoreNextResize = false;

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

    std::string getErrorName(HRESULT hr);

    void setThreadName(const char *name);
    std::string getThreadName();
}
