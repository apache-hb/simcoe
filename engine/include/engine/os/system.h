#pragma once

#include <vector>
#include <windows.h>

#include "engine/math/math.h"

namespace simcoe {
    using ResizeEvent = math::Resolution<int>;

    struct IWindowCallbacks {
        virtual ~IWindowCallbacks() = default;

        virtual void onResize(const ResizeEvent& event) { }
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
        void closeWindow();

        void doResize(int width, int height, bool fullscreen);
        void doSizeChange(WPARAM wParam, int width, int height);

        void beginUserResize();
        void endUserResize();

        bool bUserIsResizing = false;
        bool bIgnoreNextResize = false;

        HWND hWindow;
        IWindowCallbacks *pCallbacks;
    };

    struct Clock {
        Clock();

        float now() const;

    private:
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

        bool bHasNewMessage = false;
        MSG msg;
    };

    std::string getErrorName(HRESULT hr);
    std::string getWin32ErrorName(DWORD dwErrorCode);

    std::vector<std::string> getBacktrace();
    void printBacktrace();

    void setThreadName(const char *name);
    std::string getThreadName();
}
