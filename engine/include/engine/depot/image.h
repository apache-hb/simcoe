#pragma once

#include "engine/depot/vfs.h"

#include "engine/math/math.h"

namespace simcoe::depot {
    enum struct ImageFormat {
        eRGBA8
    };

    struct Image {
        Image() = default;
        Image(math::size2 size);
        Image(std::shared_ptr<IFile> pFile);

        ImageFormat format = ImageFormat::eRGBA8;
        math::size2 size = { 0, 0 };

        std::vector<std::byte> data;
    };
}
