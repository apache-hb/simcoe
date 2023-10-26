#pragma once

#include "engine/service/service.h"

#include <array>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace simcoe {
    struct TrueTypeService final : IStaticService<TrueTypeService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "truetype";
        static constexpr std::array<std::string_view, 0> kServiceDeps = {};

        // IService
        void createService() override;
        void destroyService() override;

        // TrueTypeService
        static FT_Library getLibrary() {
            return USE_SERVICE(library);
        }

    private:
        FT_Library library = nullptr;
    };
}
