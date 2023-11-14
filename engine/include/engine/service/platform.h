#pragma once

#include "engine/core/filesystem.h"

#include "engine/debug/service.h"
#include "engine/config/service.h"

#include "engine/math/math.h"

#include "engine/core/win32.h"
#include "engine/threads/queue.h"

#include <array>
#include <filesystem>

namespace simcoe {
    using WindowSize = math::Resolution<int>;

    struct IWindowCallbacks {
        virtual ~IWindowCallbacks() = default;

        virtual void onResize(const WindowSize&) { }
        virtual void onClose() { }

        virtual bool onEvent(HWND, UINT, WPARAM, LPARAM) { return false; }
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

        void showWindow();

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

        // seconds since start
        float now() const;

        // ms since start
        uint32_t ms() const;

    private:
        size_t start;
    };

    using CommandLine = std::vector<std::string>;

    namespace system {
        CommandLine getCommandLine();
    }

    struct PlatformService final : IStaticService<PlatformService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "platform";
        static inline auto kServiceDeps = depends(ConfigService::service(), DebugService::service());

        // IService
        bool createService() override;
        void destroyService() override;

        // PlatformService
        static void setup(HINSTANCE hInstance, int nCmdShow, IWindowCallbacks *pCallbacks);

        static void enqueue(std::string name, threads::WorkItem&& task);

        // win32 event loop
        static void quit(int code = 0);

        // win32 timer info
        static size_t getFrequency();
        static size_t queryCounter();

        // win32 window creation
        static Window& getWindow();
        static void showWindow();

        // directory of the executable
        static const fs::path& getExeDirectory();

        // message box
        static void message(std::string_view title, std::string_view body);
    };
}
