#include "engine/depot/service.h"

#include "engine/config/ext/builder.h"

#include "engine/log/service.h"

#include <stb/stb_image.h>

using namespace simcoe;
using namespace depot;

// handles

struct FileHandle final : IFile {
    FileHandle(std::string name, HANDLE hFile)
        : IFile(name, eRead)
        , hFile(hFile)
    { }

    ~FileHandle() override {
        CloseHandle(hFile);
    }

    size_t size() const override {
        LARGE_INTEGER liSize;
        if (!GetFileSizeEx(hFile, &liSize)) {
            debug::throwLastError("GetFileSizeEx");
        }

        return core::intCast<size_t>(liSize.QuadPart);
    }

    size_t read(void *pBuffer, size_t size) override {
        DWORD dwRead = 0;
        if (!ReadFile(hFile, pBuffer, core::intCast<DWORD>(size), &dwRead, nullptr)) {
            debug::throwLastError("ReadFile");
        }

        return dwRead;
    }

    size_t write(const void *pBuffer, size_t size) override {
        DWORD dwWritten = 0;
        if (!WriteFile(hFile, pBuffer, core::intCast<DWORD>(size), &dwWritten, nullptr)) {
            debug::throwLastError("WriteFile");
        }

        return dwWritten;
    }

private:
    HANDLE hFile = INVALID_HANDLE_VALUE;
};

namespace {
    // config options
    namespace cfg {
        DWORD waitInterval = 100; // ms

        size_t maxFileHandles = 0; // 0 = unlimited
        std::string vfsRoot = "$exe"; // default to executable directory
        depot::FileMode vfsMode = depot::eRead; // default to read only
    }

    // vfs stuff

    std::string vfsPath;

    // listen for fs changes
    HANDLE hChange = INVALID_HANDLE_VALUE;
    threads::ThreadHandle *pChangeNotify = nullptr;

    // local handles (files inside the vfs dir)
    mt::shared_mutex mutex;
    HandleMap handles;

    std::string formatPath(std::string_view plainPath) {
        // replace $cwd with current working directory
        // replace $exe with executable directory

        std::string path = std::string(plainPath);

        // replace all file separators with native ones
        for (char& c : path) {
            if (c == '/' || c == '\\') {
                c = fs::path::preferred_separator;
            }
        }

        if (size_t cwdPos = path.find("$cwd"); cwdPos != std::string::npos) {
            path.replace(cwdPos, 4, fs::current_path().string());
        } else if (size_t exePos = path.find("$exe"); exePos != std::string::npos) {
            path.replace(exePos, 4, PlatformService::getExeDirectory().string());
        }

        return path;
    }

    void notifyChange() {
        // TODO: in here lock either local or global mutex, depending on whats changing
        LOG_INFO("depot change detected");
    }
}

// service getters

mt::shared_mutex& DepotService::getMutex() { return mutex; }
HandleMap& DepotService::getHandles() { return handles; }

// service api

DepotService::DepotService() {
    CFG_DECLARE("depot",
        CFG_FIELD_INT("handles", &cfg::maxFileHandles),
        CFG_FIELD_TABLE("vfs",
            CFG_FIELD_STRING("root", &cfg::vfsRoot),
            CFG_FIELD_ENUM("mode", &cfg::vfsMode,
                CFG_CASE("readonly", eRead),
                CFG_CASE("readwrite", eReadWrite)
            )
        ),
        CFG_FIELD_TABLE("observer",
            CFG_FIELD_INT("interval", &cfg::waitInterval)
        )
    );
}

bool DepotService::createService() {
    vfsPath = formatPath(cfg::vfsRoot);
    LOG_INFO("depot vfs path: {}", vfsPath);

    constexpr DWORD dwFilter = FILE_NOTIFY_CHANGE_FILE_NAME
                             | FILE_NOTIFY_CHANGE_DIR_NAME
                             | FILE_NOTIFY_CHANGE_SIZE
                             | FILE_NOTIFY_CHANGE_LAST_WRITE;

    hChange = FindFirstChangeNotificationA(
        /*lpPathName=*/ vfsPath.c_str(),
        /*bWatchSubtree=*/ TRUE,
        /*dwNotifyFilter=*/ dwFilter
    );

    if (hChange == INVALID_HANDLE_VALUE) {
        setFailureReason("failed to create change notification handle");
        debug::throwLastError("FindFirstChangeNotificationA");
    }

    pChangeNotify = ThreadService::newThread(threads::eBackground, "depot", [](std::stop_token token) {
        while (!token.stop_requested()) {
            DWORD dwWait = WaitForSingleObject(hChange, cfg::waitInterval);
            if (dwWait == WAIT_TIMEOUT) continue;
            if (dwWait == WAIT_ABANDONED) {
                debug::throwLastError("WaitForSingleObject");
            }

            notifyChange();
            if (!FindNextChangeNotification(hChange)) {
                debug::throwLastError("FindNextChangeNotification");
            }
        }
    });

    return true;
}

void DepotService::destroyService() {
    if (hChange != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification(hChange);
    }
}

std::shared_ptr<depot::IFile> DepotService::openFile(const fs::path& path) {
    auto asset = getAssetPath(path);
    {
        mt::read_lock lock(mutex);
        if (auto it = handles.find(asset); it != handles.end()) {
            return it->second;
        }
    }

    auto str = getAssetPath(path).string();

    LOG_INFO("opening file {}", str);
    HANDLE hFile = CreateFileA(
        /*lpFileName=*/ str.c_str(),
        /*dwDesiredAccess=*/ GENERIC_READ,
        /*dwShareMode=*/ FILE_SHARE_READ,
        /*lpSecurityAttributes=*/ nullptr,
        /*dwCreationDisposition=*/ OPEN_EXISTING,
        /*dwFlagsAndAttributes=*/ FILE_ATTRIBUTE_NORMAL,
        /*hTemplateFile=*/ nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_WARN("failed to open file {}\n{}", str, debug::getErrorName());
        return nullptr;
    }

    auto handle = std::make_shared<FileHandle>(str, hFile);

    mt::write_lock lock(mutex);
    handles.emplace(path, handle);

    return handle;
}

fs::path DepotService::getAssetPath(const fs::path& path) {
    return vfsPath / path;
}

std::shared_ptr<depot::IFile> DepotService::openExternalFile(const fs::path& path) {
    {
        mt::read_lock lock(mutex);
        if (auto it = handles.find(path); it != handles.end()) {
            return it->second;
        }
    }

    auto str = path.string();

    HANDLE hFile = CreateFileA(
        /*lpFileName=*/ str.c_str(),
        /*dwDesiredAccess=*/ GENERIC_READ,
        /*dwShareMode=*/ FILE_SHARE_READ,
        /*lpSecurityAttributes=*/ nullptr,
        /*dwCreationDisposition=*/ OPEN_EXISTING,
        /*dwFlagsAndAttributes=*/ FILE_ATTRIBUTE_NORMAL,
        /*hTemplateFile=*/ nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_WARN("failed to open file {}\n{}", str, debug::getErrorName());
        return nullptr;
    }

    auto handle = std::make_shared<FileHandle>(str, hFile);

    mt::write_lock lock(mutex);
    handles.emplace(path, handle);

    return handle;
}
