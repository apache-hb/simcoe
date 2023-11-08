#include "game/entity.h"

#include "engine/core/panic.h"

using namespace game;

// entity

IEntity::IEntity(const EntityInfo& info)
    : name(info.name)
    , tag(info.tag)
    , pWorld(info.pWorld)
{ }

IEntity::~IEntity() {
    SM_ASSERTF(refs == 0, "attempting to delete entity `{}` while it still has {} live references references", name, refs.load());
}

void IEntity::track() {
    refs += 1;
}

void IEntity::untrack() {
    SM_ASSERTF(refs > 0, "attempting to untrack entity `{}` with no live references", name);
    refs -= 1;
}

// entity handle

BaseHandle::BaseHandle(IEntity *pEntity)
    : pEntity(pEntity)
{
    pEntity->track();
}

BaseHandle::~BaseHandle() {
    pEntity->untrack();
}
