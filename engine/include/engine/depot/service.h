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
        static inline auto kServiceDeps = depends(ThreadService::service());

        // IService
        bool createService() override;
        void destroyService() override;

        // open a file inside the depot
        static std::shared_ptr<depot::IFile> openFile(const fs::path& path);
        static fs::path getAssetPath(const fs::path& path);

        // global files
        static std::shared_ptr<depot::IFile> openExternalFile(const fs::path& path);

        // internal data
        static mt::SharedMutex& getMutex();
        static HandleMap& getHandles();
    };
}
