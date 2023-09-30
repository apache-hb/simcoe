#pragma once

#include <filesystem>
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

    struct Assets {
        Assets(const std::filesystem::path& root) : root(root) { }

        std::vector<std::byte> loadBlob(const std::filesystem::path& path) const;

        Image loadImage(const std::filesystem::path& path) const;
    private:
        std::filesystem::path root;
    };
}
