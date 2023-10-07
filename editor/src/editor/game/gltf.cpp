#define NOMINMAX

#include "editor/game/object.h"

#include "fastgltf/parser.hpp"
#include "fastgltf/types.hpp"

using namespace editor;

namespace fg = fastgltf;

namespace {
    constexpr auto kLoadOpts = fg::Options::LoadExternalBuffers
                             | fg::Options::LoadExternalImages;
    bool load(std::filesystem::path path) {
        fg::Parser parser;
        fg::GltfDataBuffer dataBuffer;
        dataBuffer.loadFromFile(path);

        fg::Expected<fg::Asset> asset(fg::Error::None);

        switch (fg::determineGltfFileType(&dataBuffer)) {
        case fg::GltfType::glTF:
            asset = parser.loadGLTF(&dataBuffer, path.parent_path(), kLoadOpts);
            break;
        case fg::GltfType::GLB:
            asset = parser.loadBinaryGLTF(&dataBuffer, path.parent_path(), kLoadOpts);
            break;
        default:
            simcoe::logError("failed to determine gltf file type {}", path.string());
            return false;
        }

        if (asset.error() != fg::Error::None) {
            simcoe::logError("failed to load gltf {}", path.string());
            simcoe::logError("{}", fg::getErrorMessage(asset.error()));
            return false;
        }

        return true;
    }
}

void GltfMesh::create() {

}

void GltfMesh::destroy() {

}
