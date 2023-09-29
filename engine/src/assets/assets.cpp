#include "engine/assets/assets.h"

#include <fstream>

using namespace simcoe::assets;

std::vector<std::byte> Assets::load(const std::filesystem::path& path) const {
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
