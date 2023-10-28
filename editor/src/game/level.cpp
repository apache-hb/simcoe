#include "game/level.h"

#include "game/world.h"

#include "game/entity.h"

using namespace game;

ILevel::ILevel(const LevelInfo& info)
    : pWorld(info.pWorld)
    , entities(new EntityPtr[info.entityLimit])
    , indices(info.entityLimit)
{ }

void ILevel::retireEntity(const EntityTag& tag) {
    std::lock_guard guard(lock);
    retired.emplace(tag);
}

// world lifecycle

void ILevel::beginTick() {
    std::lock_guard guard(lock);
    for (EntityTag tag : retired) {
        IEntity *pEntity = getEntityInner(tag);
        indices.release(tag.slot, tag.version);
        delete pEntity;
    }

    retired.clear();
}

void ILevel::endTick() {
    std::lock_guard guard(lock);
    for (IEntity *pEntity : staged) {
        entities[EntitySlotType(pEntity->getSlot())] = pEntity;
    }
}

// entity creation

EntityTag ILevel::newEntityIndex() {
    EntityVersion version = pWorld->newEntityVersion();
    EntitySlot slot = indices.alloc(version);

    ASSERTF(slot != EntitySlot::eInvalid, "failed to allocate entity index");

    return { slot, version };
}

EntityInfo ILevel::newEntityInfo(const EntityTag& tag, std::string_view name) {
    EntityInfo info = {
        .name = std::string(name),
        .tag = tag,
        .pWorld = pWorld,
        .pLevel = this
    };

    return info;
}

// entity sanity checks

IEntity *ILevel::getEntityInner(const EntityTag& tag) {
    ASSERTF(tag.slot != EntitySlot::eInvalid, "attempting to access invalid entity: {}", tag);
    ASSERTF(indices.test(tag.slot, tag.version), "attempting to access stale entity: {}", tag);

    IEntity *pEntity = entities[EntitySlotType(tag.slot)];
    ASSERTF(!isEntityStale(pEntity, tag.version), "entity is stale: (entity={}, world={})", pEntity->getTag(), tag);

    return pEntity;
}

bool ILevel::isEntityStale(const IEntity *pEntity, EntityVersion version) const {
    return pEntity->getVersion() != version;
}
