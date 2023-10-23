#pragma once

#include <filesystem>
#include <vector>
#include <unordered_map>

namespace simcoe::assets {
    namespace fs = std::filesystem;

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
        Assets(const fs::path& root) : root(root) { }

        fs::path getAssetPath(const fs::path& path) const;

        std::vector<std::byte> loadBlob(const fs::path& path) const;

        Image loadImage(const fs::path& path) const;
    private:
        fs::path root;
    };
}
