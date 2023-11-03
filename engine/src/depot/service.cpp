#include "engine/depot/service.h"

#include "engine/config/ext/builder.h"

#include "engine/log/service.h"

#include <stb/stb_image.h>

using namespace simcoe;
using namespace depot;

// handles

struct FileHandle final : IFile {
    FileHandle(HANDLE hFile)
        : IFile(eRead)
        , hFile(hFile)
    { }

    ~FileHandle() override {
        CloseHandle(hFile);
    }

    size_t size() const override {
        LARGE_INTEGER liSize;
        if (!GetFileSizeEx(hFile, &liSize)) {
            throwLastError("GetFileSizeEx");
        }

        return core::intCast<size_t>(liSize.QuadPart);
    }

    size_t read(void *pBuffer, size_t size) override {
        DWORD dwRead = 0;
        if (!ReadFile(hFile, pBuffer, core::intCast<DWORD>(size), &dwRead, nullptr)) {
            throwLastError("ReadFile");
        }

        return dwRead;
    }

    size_t write(const void *pBuffer, size_t size) override {
        DWORD dwWritten = 0;
        if (!WriteFile(hFile, pBuffer, core::intCast<DWORD>(size), &dwWritten, nullptr)) {
            throwLastError("WriteFile");
        }

        return dwWritten;
    }

private:
    HANDLE hFile = INVALID_HANDLE_VALUE;
};

// service

DepotService::DepotService() {
    CFG_DECLARE("depot",
        CFG_FIELD_INT("handles", &maxHandles),
        CFG_FIELD_TABLE("vfs",
            CFG_FIELD_STRING("root", &userVfsPath),
            CFG_FIELD_ENUM("mode", &vfsMode,
                CFG_CASE("readonly", eRead),
                CFG_CASE("readwrite", eReadWrite)
            )
        )
    );
}

bool DepotService::createService() {
    vfsPath = formatVfsPath();
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
        throwLastError("FindFirstChangeNotificationA");
    }

    pChangeNotify = ThreadService::newThread(threads::eBackground, "depot", [this](std::stop_token token) {
        while (!token.stop_requested()) {
            DWORD dwWait = WaitForSingleObject(hChange, 100);
            if (dwWait == WAIT_TIMEOUT) continue;
            if (dwWait == WAIT_ABANDONED) {
                throwLastError("WaitForSingleObject");
            }

            std::lock_guard lock(vfsMutex);
            notifyChange();
            if (!FindNextChangeNotification(hChange)) {
                throwLastError("FindNextChangeNotification");
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

std::string_view DepotService::getFailureReason() {
    return get()->failureReason;
}

std::shared_ptr<depot::IFile> DepotService::openFile(const fs::path& path) {
    {
        mt::read_lock lock(get()->vfsMutex);
        auto it = get()->handles.find(path);
        if (it != get()->handles.end()) {
            return it->second;
        }
    }

    LOG_INFO("opening file {}", path.string());
    HANDLE hFile = CreateFileA(
        /*lpFileName=*/ path.string().c_str(),
        /*dwDesiredAccess=*/ GENERIC_READ,
        /*dwShareMode=*/ FILE_SHARE_READ,
        /*lpSecurityAttributes=*/ nullptr,
        /*dwCreationDisposition=*/ OPEN_EXISTING,
        /*dwFlagsAndAttributes=*/ FILE_ATTRIBUTE_NORMAL,
        /*hTemplateFile=*/ nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_WARN("failed to open file {}\n{}", path.string(), DebugService::getErrorName());
        return nullptr;
    }

    auto handle = std::make_shared<FileHandle>(hFile);

    mt::write_lock lock(get()->vfsMutex);
    get()->handles.emplace(path, handle);

    return handle;
}

std::vector<std::byte> DepotService::loadBlob(const fs::path& path) {
    auto pFile = openFile(path);
    if (pFile == nullptr) {
        return {};
    }

    return pFile->blob();
}

fs::path DepotService::getAssetPath(const fs::path& path) {
    return get()->vfsPath / path;
}

depot::Image DepotService::loadImage(const fs::path &path) {
    constexpr size_t kChannels = 4;

    fs::path fullPath = getAssetPath(path);

    int width;
    int height;

    stbi_uc *pData = stbi_load(fullPath.string().c_str(), &width, &height, nullptr, kChannels);

    if (!pData) {
        throw std::runtime_error("Failed to load image: " + fullPath.string());
    }

    // round up to nearest power of 2
    int newWidth = core::nextPowerOf2(width);
    int newHeight = core::nextPowerOf2(height);

    newWidth = newHeight = std::max(newWidth, newHeight);
    std::vector<std::byte> newData(newWidth * newHeight * kChannels);

    // copy image data, try and center it in the new image
    for (int y = 0; y < height; ++y) {
        std::memcpy(newData.data() + (newWidth * (y + (newHeight - height) / 2) + (newWidth - width) / 2) * kChannels,
                    pData + width * y * kChannels,
                    width * kChannels);
    }

    Image image = {
        .format = ImageFormat::eRGBA8,
        .width = static_cast<size_t>(newWidth),
        .height = static_cast<size_t>(newHeight),
        .data = std::move(newData)
    };

    stbi_image_free(pData);

    return image;
}

depot::Font DepotService::loadFont(const fs::path& path) {
    auto ttf = getAssetPath(path);
    ttf.replace_extension("ttf");
    if (!fs::exists(ttf)) {
        LOG_ASSERT("font file `{}` does not exist", ttf.string());
    }

    return Font(ttf.string().c_str());
}

std::shared_ptr<depot::IFile> DepotService::openExternalFile(const fs::path& path) {
    {
        mt::read_lock lock(get()->globalMutex);
        auto it = get()->globalHandles.find(path);
        if (it != get()->globalHandles.end()) {
            return it->second;
        }
    }

    HANDLE hFile = CreateFileA(
        /*lpFileName=*/ path.string().c_str(),
        /*dwDesiredAccess=*/ GENERIC_READ,
        /*dwShareMode=*/ FILE_SHARE_READ,
        /*lpSecurityAttributes=*/ nullptr,
        /*dwCreationDisposition=*/ OPEN_EXISTING,
        /*dwFlagsAndAttributes=*/ FILE_ATTRIBUTE_NORMAL,
        /*hTemplateFile=*/ nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_WARN("failed to open file {}\n{}", path.string(), DebugService::getErrorName());
        return nullptr;
    }

    auto handle = std::make_shared<FileHandle>(hFile);

    mt::write_lock lock(get()->globalMutex);
    get()->globalHandles.emplace(path, handle);

    return handle;
}

std::string DepotService::formatVfsPath() const {
    // replace $cwd with current working directory
    // replace $exe with executable directory

    std::string path = userVfsPath;

    // replace all file separators with native ones
    for (char& c : path) {
        if (c == '/' || c == '\\') {
            c = fs::path::preferred_separator;
        }
    }

    if (size_t pos = path.find("$cwd"); pos != std::string::npos) {
        path.replace(pos, 4, fs::current_path().string());
    }

    if (size_t pos = path.find("$exe"); pos != std::string::npos) {
        path.replace(pos, 4, PlatformService::getExeDirectory().string());
    }

    return path;
}

void DepotService::notifyChange() {
    LOG_INFO("depot change detected");
}
