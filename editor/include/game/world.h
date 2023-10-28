#pragma once

#include "game/entity.h"

#include <random>

namespace game {
    struct EntityIndex {
        size_t index;
        size_t version;
    };

    struct World {
        template<std::derived_from<IEntity> T, typename... A>
            requires std::is_constructible_v<T, EntityInfo>
        EntityIndex newEntity(A&&... args) {

        }

    private:
        std::mutex lock; // game engine lock to ensure thread safety

        // rng for generating entity ids
        std::mt19937_64 rng;
        std::uniform_int_distribution<size_t> dist{0, SIZE_MAX};


        // entity lifecycle management
        std::unordered_set<IEntity*> staged; // entities to be added next tick
        std::unordered_set<IEntity*> retired; // entities to be removed next tick

        std::vector<IEntity*> entities; // all entities currently active in the world
    };
}
