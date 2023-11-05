#include "engine/depot/vfs.h"

using namespace simcoe;
using namespace simcoe::depot;

std::vector<std::byte> IFile::blob() {
    std::vector<std::byte> data;

    while (true) {
        char buffer[1024];
        size_t bytes = read(buffer, sizeof(buffer));
        std::byte *ptr = reinterpret_cast<std::byte *>(buffer);
        data.insert(data.end(), ptr, ptr + bytes);
        if (bytes < sizeof(buffer)) {
            break;
        }
    }

    return data;
}
