#pragma once

#include "game/ecs/typeinfo.h"

namespace game {
    struct ObjectStorage;
    struct StorageIter;

    struct ObjectStorage {
        ObjectStorage(TypeInfo info, size_t size);

        Index allocate();
        void release(Index index);

        void insert(Index index, ObjectPtr pObject);
        ObjectPtr get(Index index) const;

        StorageIter begin();
        StorageIter end();

        size_t getUsed() const { return alloc.countSetBits(); }
        size_t getSize() const { return alloc.getTotalBits(); }
        bool isAllocated(Index index) const { return alloc.test(index); }

        ObjectPtr at(Index index) const { return objects[size_t(index)]; }

    private:
        TypeInfo info;
        simcoe::core::UniquePtr<ObjectPtr[]> objects;
        simcoe::core::BitMap alloc;
    };

    struct StorageIter {
        StorageIter(ObjectStorage *pStorage, size_t index);

        StorageIter& operator++();
        ObjectPtr operator*() const;

        bool operator==(const StorageIter& other) const;
        operator bool() const { return index < pStorage->getSize(); }

    private:
        ObjectStorage *pStorage = nullptr;
        size_t index = 0;
    };
}
