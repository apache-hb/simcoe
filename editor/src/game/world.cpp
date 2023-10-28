#include "game/world.h"

#include "game/entity.h"

using namespace game;

World::World(const WorldInfo& info)
    : rng(info.seed)
    , entityLimit(info.entityLimit)
{ }

EntityVersion World::newEntityVersion() {
    std::lock_guard guard(lock);
    return EntityVersion(dist(rng));
}
