#include "engine/depot/vfs.h"

using namespace simcoe;
using namespace simcoe::depot;

std::vector<std::byte> IFile::blob() {
    std::vector<std::byte> data;
    data.resize(size());
    read(data.data(), data.size());
    return data;
}
