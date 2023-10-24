#pragma once

#include <filesystem>
#include <vector>
#include <unordered_map>

#include "engine/assets/font.h"

namespace simcoe::assets {
    namespace fs = std::filesystem;

    struct Assets {
        Assets(const fs::path& root) : root(root) { }

        fs::path getAssetPath(const fs::path& path) const;

        std::vector<std::byte> loadBlob(const fs::path& path) const;

        Image loadImage(const fs::path& path) const;

        Font loadFont(const fs::path& path) const;
        Font loadSystemFont(std::string_view name) const;
    private:
        fs::path root;
    };
}
