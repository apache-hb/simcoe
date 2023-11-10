#pragma once

#include "engine/service/service.h"

#include <array>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace simcoe {
    struct FreeTypeService final : IStaticService<FreeTypeService> {
        FreeTypeService();

        // IStaticService
        static constexpr std::string_view kServiceName = "freetype";
        static inline auto kServiceDeps = depends();

        // IService
        bool createService() override;
        void destroyService() override;

        // FreeTypeService
        static FT_Library getLibrary();
    };
}
