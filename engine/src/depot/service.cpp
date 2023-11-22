#include "engine/depot/service.h"

#include "engine/core/error.h"
#include "engine/core/units.h"
#include "engine/log/service.h"

#include "engine/config/system.h"

#include <stb/stb_image.h>

using namespace simcoe;
using namespace depot;

// handles

static DWORD remapMoveMethod(SeekMode mode) {
    switch (mode) {
    case eAbsolute: return FILE_BEGIN;
    case eCurrent: return FILE_CURRENT;
    case eEnd: return FILE_END;
    default: core::throwFatal("invalid seek mode: {}", int(mode));
    }
}

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

    size_t seek(size_t offset, SeekMode seek) override {
        LARGE_INTEGER liOffset;
        liOffset.QuadPart = core::intCast<LONGLONG>(offset);

        LARGE_INTEGER liNewPos;
        if (!SetFilePointerEx(hFile, liOffset, &liNewPos, remapMoveMethod(seek))) {
            debug::throwLastError("SetFilePointerEx");
        }

        return core::intCast<size_t>(liNewPos.QuadPart);
    }

    size_t tell() const override {
        LARGE_INTEGER liPos;
        liPos.QuadPart = 0;

        LARGE_INTEGER liNewPos;
        if (!SetFilePointerEx(hFile, liPos, &liNewPos, FILE_CURRENT)) {
            debug::throwLastError("SetFilePointerEx");
        }

        return core::intCast<size_t>(liNewPos.QuadPart);
    }

private:
    HANDLE hFile = INVALID_HANDLE_VALUE;
};

config::ConfigValue<size_t> cfgWaitInterval("depot", "wait_interval", "Interval between depot change notifications (in ms)", 100);
config::ConfigValue<size_t> cfgMaxFileHandles("depot", "max_file_handles", "Maximum number of file handles to keep open (0 = unlimited)", 128);

config::ConfigValue<std::string> cfgVfsRoot("depot/vfs", "root", "Root directory for the virtual file system", "$exe");

const config::ConfigEnumMap kModeFlags = {
    { "read", eRead },
    { "readwrite", eReadWrite }
};

config::ConfigValue<depot::FileMode> cfgVfsMode("depot/vfs", "mode", "Mode for the virtual file system", depot::eRead, kModeFlags);

namespace {
    // vfs stuff

    std::string vfsPath;

    // listen for fs changes
    HANDLE hChange = INVALID_HANDLE_VALUE;
    threads::ThreadHandle *pChangeNotify = nullptr;

    // local handles (files inside the vfs dir)
    mt::SharedMutex gMutex{"vfs"};
    HandleMap gHandles;

    std::string formatVfsPath(std::string_view plainPath) {
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

mt::SharedMutex& DepotService::getMutex() { return gMutex; }
HandleMap& DepotService::getHandles() { return gHandles; }

// service api

bool DepotService::createService() {
    vfsPath = formatVfsPath(cfgVfsRoot.getCurrentValue());
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
        LOG_ERROR("failed to create change notification handle at `{}`", vfsPath);
        setFailureReason("failed to create change notification handle");
        debug::throwLastError("FindFirstChangeNotificationA");
    }

    pChangeNotify = ThreadService::newThread(threads::eBackground, "depot", [](std::stop_token token) {
        while (!token.stop_requested()) {
            DWORD dwWait = WaitForSingleObject(hChange, core::intCast<DWORD>(cfgWaitInterval.getCurrentValue()));
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
    pChangeNotify->join();
    
    if (hChange != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification(hChange);
    }
}

std::shared_ptr<depot::IFile> DepotService::openFile(const fs::path& path) {
    auto asset = getAssetPath(path);
    {
        mt::ReadLock lock(gMutex);
        if (auto it = gHandles.find(asset); it != gHandles.end()) {
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

    mt::WriteLock lock(gMutex);
    gHandles.emplace(path, handle);

    return handle;
}

fs::path DepotService::getAssetPath(const fs::path& path) {
    return vfsPath / path;
}

fs::path DepotService::formatPath(const fs::path &plainPath) {
    std::string path = plainPath.string();

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
    } else if (size_t vfsPos = path.find("$vfs"); vfsPos != std::string::npos) {
        path.replace(vfsPos, 4, vfsPath);
    }

    return path;
}

std::shared_ptr<depot::IFile> DepotService::openExternalFile(const fs::path& path) {
    {
        mt::ReadLock lock(gMutex);
        if (auto it = gHandles.find(path); it != gHandles.end()) {
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

    mt::WriteLock lock(gMutex);
    gHandles.emplace(path, handle);

    return handle;
}
