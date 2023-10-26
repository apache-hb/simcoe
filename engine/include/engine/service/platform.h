#pragma once

#include "engine/service/debug.h"

#include "engine/math/math.h"

#include <array>
#include <windows.h>

namespace simcoe {
    using WindowSize = math::Resolution<int>;

    struct IWindowCallbacks {
        virtual ~IWindowCallbacks() = default;

        virtual void onResize(const WindowSize& event) { }
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
        WindowSize size;

        IWindowCallbacks *pCallbacks;
    };

    struct Window {
        Window(const WindowCreateInfo& createInfo);
        ~Window();

        HWND getHandle() const;
        WindowSize getSize() const;
        RECT getWindowCoords() const;
        RECT getClientCoords() const;

        void enterFullscreen();
        void exitFullscreen();

        void setStyle(WindowStyle style);

        static LRESULT CALLBACK callback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    private:
        void closeWindow();

        void doResize(int width, int height);
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

    struct PlatformService final : IStaticService<PlatformService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "platform";
        static constexpr std::array<std::string_view, 0> kServiceDeps = { DebugService::kServiceName };

        // IService
        void createService() override;
        void destroyService() override;

        // PlatformService
        static void setup(HINSTANCE hInstance, int nCmdShow);

        // PlatformService win32 event loop
        static bool getEvent();
        static void dispatchEvent();
        static void quit(int code = 0);

        // PlatformService win32 timer info
        static size_t getFrequency() { return USE_SERVICE(frequency); }
        static size_t queryCounter();

        // PlatformService win32 window creation
        static HINSTANCE getInstanceHandle() { return USE_SERVICE(hInstance); }
        static int getShowCmd() { return USE_SERVICE(nCmdShow); }

    private:
        HINSTANCE hInstance = nullptr;
        int nCmdShow = -1;

        MSG msg = {};

        size_t frequency = 0;
    };
}
