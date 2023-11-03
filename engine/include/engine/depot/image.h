#pragma once

#include "engine/service/freetype.h"

#include "engine/core/filesystem.h"
#include "engine/core/utf8.h"

#include "engine/math/math.h"

#include <span>

namespace simcoe::depot {
    enum struct ImageFormat {
        eRGBA8
    };

    struct Image {
        ImageFormat format;
        size_t width;
        size_t height;

        std::vector<std::byte> data;
    };
}
