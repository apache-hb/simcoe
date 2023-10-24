#pragma once

#include <vector>

namespace simcoe::assets {
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
