#include "engine/assets/assets.h"

#include <stb/stb_image.h>

#include <fstream>

using namespace simcoe::assets;

namespace {
    constexpr size_t kChannels = 4;
}

std::vector<std::byte> Assets::loadBlob(const std::filesystem::path& path) const {
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

Image Assets::loadImage(const std::filesystem::path& path) const {
    std::filesystem::path fullPath = root / path;

    int width;
    int height;

    stbi_uc *pData = stbi_load(fullPath.string().c_str(), &width, &height, nullptr, kChannels);

    if (!pData) {
        throw std::runtime_error("Failed to load image: " + fullPath.string());
    }

    Image image = {
        .format = ImageFormat::eRGBA8,
        .width = static_cast<size_t>(width),
        .height = static_cast<size_t>(height),
        .data = { (std::byte*)pData, (std::byte*)pData + width * height * kChannels }
    };

    stbi_image_free(pData);

    return image;
}
