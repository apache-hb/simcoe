#pragma once

#include "engine/service/debug.h"

#include "engine/math/math.h"

#include "engine/core/win32.h"

#include <array>
#include <filesystem>

namespace simcoe {
    using WindowSize = math::Resolution<int>;
    namespace fs = std::filesystem;

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
        SM_NOCOPY(Window)

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

    using CommandLine = std::vector<std::string>;

    struct PlatformService final : IStaticService<PlatformService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "platform";
        static constexpr std::array kServiceDeps = { DebugService::kServiceName };

        // IService
        bool createService() override;
        void destroyService() override;

        // PlatformService
        static void setup(HINSTANCE hInstance, int nCmdShow);
        static CommandLine getCommandLine();

        // win32 event loop
        static bool getEvent();
        static bool waitForEvent();
        static void dispatchEvent();
        static void quit(int code = 0);

        // win32 timer info
        static size_t getFrequency();
        static size_t queryCounter();

        // win32 window creation
        static HINSTANCE getInstanceHandle();
        static int getShowCmd();

        // directory of the executable
        static fs::path getExeDirectory();

        // message box
        static void message(std::string_view title, std::string_view body);

    private:
        HINSTANCE hInstance = nullptr;
        int nCmdShow = -1;

        MSG msg = {};

        size_t frequency = 0;

        fs::path exeDirectory;
    };
}
