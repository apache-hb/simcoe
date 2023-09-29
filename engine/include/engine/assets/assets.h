#pragma once

#include <filesystem>
#include <vector>

namespace simcoe::assets {
    struct Assets {
        Assets(const std::filesystem::path& root) : root(root) { }

        std::vector<std::byte> load(const std::filesystem::path& path) const;
    private:
        std::filesystem::path root;
    };
}
