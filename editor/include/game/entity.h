#pragma once

#include "game/info.h"

#include <stddef.h>
#include <atomic>
#include <string>

namespace game {
    struct IEntity {
        IEntity(const EntityInfo& info);
        virtual ~IEntity();

        // entity lifecycle
        virtual void tick(float delta) = 0;

        // getters
        EntityTag getTag() const { return tag; }
        EntitySlot getSlot() const { return tag.slot; }
        EntityVersion getVersion() const { return tag.version; }

        // ref tracking
        void track();
        void untrack();

    private:
        // name and current tag
        std::string name;
        EntityTag tag;

        World *pWorld;

        // ref tracking
        // TODO: track handles by thread as well
        std::atomic_size_t refs = 0;
    };

    struct BaseHandle {
        BaseHandle(IEntity *pEntity);
        ~BaseHandle();

        operator IEntity *() const { return pEntity; }

    private:
        IEntity *pEntity;
    };

    template<std::derived_from<IEntity> T>
    struct EntityHandle : BaseHandle {
        using BaseHandle::BaseHandle;

        operator T *() const { return static_cast<T*>(*this); }
        T *operator->() const { return static_cast<T*>(*this); }
    };
}
