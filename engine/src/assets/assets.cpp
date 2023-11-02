#include "engine/assets/assets.h"

#include "engine/core/units.h"
#include "engine/service/logging.h"

#include <stb/stb_image.h>

#include <fstream>

using namespace simcoe::assets;

namespace {
    constexpr size_t kChannels = 4;

    const fs::path kSystemFontDir = "C:\\Windows\\Fonts";

    Font getFontFile(fs::path path) {
        path.replace_extension("ttf");
        if (!fs::exists(path)) {
            LOG_ASSERT("font file `{}` does not exist", path.string());
        }

        return Font(path.string().c_str());
    }
}

fs::path Assets::getAssetPath(const fs::path& path) const {
    return root / path;
}

std::vector<std::byte> Assets::loadBlob(const fs::path& path) const {
    std::vector<std::byte> data;

    std::ifstream file(root / path, std::ios::binary);
    if (file.is_open()) {
        file.seekg(0, std::ios::end);
        data.resize(file.tellg());
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(data.data()), data.size());
        file.close();
    }

    return data;
}

Image Assets::loadImage(const fs::path& path) const {
    fs::path fullPath = root / path;

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

Font Assets::loadFont(const fs::path& path) const {
    return getFontFile(root / path);
}

Font Assets::loadSystemFont(std::string_view name) const {
    return getFontFile(kSystemFontDir / name);
}
