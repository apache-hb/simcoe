#pragma once

#include "engine/core/filesystem.h"

namespace simcoe::depot {
    enum FileMode {
        eRead,
        eReadWrite,

        eCount
    };

    struct IFile {
        virtual ~IFile() = default;

        IFile(FileMode mode) : mode(mode) { }

        virtual size_t size() const = 0;
        virtual size_t read(void *pBuffer, size_t size) = 0;
        virtual size_t write(const void *pBuffer, size_t size) = 0;

        std::vector<std::byte> blob();

        FileMode getMode() const { return mode; }

    private:
        FileMode mode = eRead;
    };

    struct IFolder {
        virtual ~IFolder() = default;

        virtual IFile *openFile(std::string_view path) = 0;
        virtual IFolder *openFolder(std::string_view path) = 0;
    };
}
