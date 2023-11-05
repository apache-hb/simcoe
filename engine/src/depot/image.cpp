#include "engine/depot/image.h"

#include "engine/core/units.h"

#include <stb/stb_image.h>

using namespace simcoe;
using namespace simcoe::depot;

namespace {
    constexpr size_t kChannels = 4;
}

Image::Image(math::size2 size)
    : size(size)
    , data(size.width * size.height * kChannels)
{ }

Image::Image(std::shared_ptr<IFile> pFile) {
    auto blob = pFile->blob();

    int blobSize = core::intCast<int>(blob.size());
    const stbi_uc *pBuffer = reinterpret_cast<stbi_uc *>(blob.data());

    int imageWidth;
    int imageHeight;

    stbi_uc *pData = stbi_load_from_memory(pBuffer, blobSize, &imageWidth, &imageHeight, nullptr, kChannels);

    if (pData == nullptr) {
        throw std::runtime_error(std::format("Failed to load image: {}", pFile->getName()));
    }

    // round up to nearest power of 2
    int newWidth = core::nextPowerOf2(imageWidth);
    int newHeight = core::nextPowerOf2(imageHeight);

    newWidth = newHeight = std::max(newWidth, newHeight);
    data.resize(newWidth * newHeight * kChannels);

    // copy image data, try and center it in the new image
    for (int y = 0; y < imageHeight; ++y) {
        std::memcpy(data.data() + (newWidth * (y + (newHeight - imageHeight) / 2) + (newWidth - imageWidth) / 2) * kChannels,
                    pData + imageWidth * y * kChannels,
                    imageWidth * kChannels);
    }

    format = ImageFormat::eRGBA8;
    size = { core::intCast<size_t>(newWidth), core::intCast<size_t>(newHeight) };

    stbi_image_free(pData);
}
