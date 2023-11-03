#pragma once

#include "engine/service/service.h"
#include "engine/threads/service.h"

#include "engine/depot/vfs.h"
#include "engine/depot/font.h"
#include "engine/depot/image.h"

#include <array>

namespace simcoe {
    using HandleMap = std::unordered_map<fs::path, std::shared_ptr<depot::IFile>>;

    struct DepotService final : IStaticService<DepotService> {
        DepotService();

        // IStaticService
        static constexpr std::string_view kServiceName = "depot";
        static constexpr std::array kServiceDeps = { ThreadService::kServiceName };

        // IService
        bool createService() override;
        void destroyService() override;

        // failure
        static std::string_view getFailureReason();

        // vfs
        static std::shared_ptr<depot::IFile> openFile(const fs::path& path);
        static std::vector<std::byte> loadBlob(const fs::path& path);
        static fs::path getAssetPath(const fs::path& path);
        static depot::Image loadImage(const fs::path& path);
        static depot::Font loadFont(const fs::path& path);

        // files outside of the vfs
        static std::shared_ptr<depot::IFile> openExternalFile(const fs::path& path);

    private:
        std::string failureReason;

        // config
        size_t maxHandles = 0; // 0 = unlimited
        std::string userVfsPath;
        depot::FileMode vfsMode = depot::eRead;

        std::string vfsPath;

        // listen for fs changes
        HANDLE hChange = INVALID_HANDLE_VALUE;
        threads::ThreadHandle *pChangeNotify = nullptr;

        void notifyChange();
        std::string formatVfsPath() const;

        // vfs handles
        mt::shared_mutex vfsMutex;
        HandleMap handles;

        // global handles
        mt::shared_mutex globalMutex;
        HandleMap globalHandles;

    public:
        static mt::shared_mutex& getVfsMutex() { return get()->vfsMutex; }
        static mt::shared_mutex& getGlobalMutex() { return get()->globalMutex; }

        static HandleMap& getHandles() { return get()->handles; }
        static HandleMap& getGlobalHandles() { return get()->globalHandles; }
    };
}
