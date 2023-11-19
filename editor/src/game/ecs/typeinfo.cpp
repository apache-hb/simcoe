#include "game/ecs/typeinfo.h"

using namespace game;

TypeInfo::TypeInfo(World *pWorld, size_t id)
    : id(id) 
#if SM_DEBUG
    , pWorld(pWorld)
#endif
{ }

bool TypeInfo::operator==(const TypeInfo& other) const { 
#if SM_DEBUG
    SM_ASSERTF(pWorld == other.pWorld, "comparing ids {}, {} from different worlds {} and {}", id, other.id, (void*)pWorld, (void*)other.pWorld);
#endif
    return id == other.id;
}
