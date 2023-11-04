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

        // vfs
        static std::shared_ptr<depot::IFile> openFile(const fs::path& path);
        static std::vector<std::byte> loadBlob(const fs::path& path);
        static fs::path getAssetPath(const fs::path& path);
        static depot::Image loadImage(const fs::path& path);
        static depot::Font loadFont(const fs::path& path);

        // global files
        static std::shared_ptr<depot::IFile> openExternalFile(const fs::path& path);

        // internal data
        static mt::shared_mutex& getVfsMutex();
        static mt::shared_mutex& getGlobalMutex();

        static HandleMap& getHandles();
        static HandleMap& getGlobalHandles();
    };
}
