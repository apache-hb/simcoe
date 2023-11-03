#pragma once

#include "game/entity.h"

#include "engine/core/array.h"
#include "engine/service/platform.h"

#include <unordered_set>
#include <random>

namespace game {
    using EntityPtr = IEntity*;

    struct ILevel {
        ILevel(const LevelInfo& info);

        template<std::derived_from<IEntity> T, typename... A> requires std::is_constructible_v<T, EntityInfo, A...>
        EntityTag newEntity(A&&... args) {
            std::lock_guard guard(lock);

            // create version data
            EntityTag entityIndex = newEntityIndex();
            EntityInfo createInfo = newEntityInfo(entityIndex, "entity");

            T *pEntity = new T(createInfo, args...);
            staged.emplace(pEntity);
            return entityIndex;
        }

        template<std::derived_from<IEntity> T>
        EntityHandle<T> getEntity(const EntityTag& index) {
            std::lock_guard guard(lock);
            return static_cast<T*>(getEntityInner(index));
        }

        void retireEntity(const EntityTag& tag);

        void beginTick();
        void endTick();

    private:
        World *pWorld;
        // game engine lock to ensure thread safety
        std::mutex lock;

        // timekeeping
        Clock clock;

        EntityTag newEntityIndex();
        EntityInfo newEntityInfo(const EntityTag& index, std::string_view name);
        IEntity *getEntityInner(const EntityTag& index);

        bool isEntityStale(const IEntity *pEntity, EntityVersion version) const;

        // entity bookkeeping
        std::unordered_set<EntityPtr> staged; // entities to be added next tick
        std::unordered_set<EntityTag> retired; // entities to be removed next tick

        core::UniquePtr<EntityPtr[]> entities; // all entities currently in the world
        EntitySlotMap indices; // entity slot allocator
    };
}
