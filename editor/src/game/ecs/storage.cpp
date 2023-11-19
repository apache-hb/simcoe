#include "game/ecs/storage.h"

#include "engine/log/service.h"
#include "game/ecs/objects.h"

using namespace game;

ObjectStorage::ObjectStorage(TypeInfo info, size_t size)
    : info(info)
    , objects(size)
    , alloc(size)
{ }

Index ObjectStorage::allocate() {
    Index index = alloc.alloc();
    SM_ASSERTF(index != Index::eInvalid, "storage {} is full", info.getId());
    return index;
}

void ObjectStorage::release(Index index) {
    alloc.release(index);
    delete objects[size_t(index)];
}

void ObjectStorage::insert(Index index, ObjectPtr pObject) {
    SM_ASSERTF(isAllocated(index), "storage {} index {} is not allocated", info.getId(), size_t(index));

    objects[size_t(index)] = pObject;
}

ObjectPtr ObjectStorage::get(Index index) const {
    SM_ASSERTF(isAllocated(index), "storage {} index {} is not allocated", info.getId(), size_t(index));

    return objects[size_t(index)];
}

StorageIter ObjectStorage::begin() { return StorageIter(this, 0); }
StorageIter ObjectStorage::end() { return StorageIter(this, alloc.getTotalBits()); }

StorageIter::StorageIter(ObjectStorage *pStorage, size_t index)
    : pStorage(pStorage)
    , index(index)
{ }

StorageIter& StorageIter::operator++() {
    do {
        ++index;
    } while (index < pStorage->getSize() && !pStorage->isAllocated(Index(index)));

    return *this;
}

ObjectPtr StorageIter::operator*() const { 
    return pStorage->at(Index(index)); 
}

bool StorageIter::operator==(const StorageIter& other) const { 
    SM_ASSERTF(pStorage == other.pStorage, "comparing iterators from different storages {} and {}", (void*)pStorage, (void*)other.pStorage);
    return index == other.index; 
}
