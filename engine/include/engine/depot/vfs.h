#pragma once

#include "engine/core/filesystem.h"

namespace simcoe::depot {
    enum FileMode {
        eRead,
        eReadWrite,

        eCount
    };

    enum SeekMode {
        eAbsolute,
        eCurrent,
        eEnd
    };

    struct IFile {
        virtual ~IFile() = default;

        IFile(std::string name, FileMode mode)
            : name(name)
            , mode(mode)
        { }

        virtual size_t size() const = 0;
        virtual size_t read(void *pBuffer, size_t size) = 0;
        virtual size_t write(const void *pBuffer, size_t size) = 0;
        virtual size_t seek(size_t offset, SeekMode seek) = 0;
        virtual size_t tell() const = 0;

        std::vector<std::byte> blob();

        FileMode getMode() const { return mode; }
        std::string_view getName() const { return name; }

    private:
        std::string name;
        FileMode mode = eRead;
    };

    struct IFolder {
        virtual ~IFolder() = default;

        virtual IFile *openFile(std::string_view path) = 0;
        virtual IFolder *openFolder(std::string_view path) = 0;
    };
}
