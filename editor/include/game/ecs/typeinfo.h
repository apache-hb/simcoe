#pragma once

#include "engine/core/panic.h"
#include "engine/core/bitmap.h"

#include <compare>

namespace game {
    struct World;
    struct IObject;
    struct IEntity;
    struct IComponent;
    
    using Index = simcoe::core::BitMap::Index;

    using ObjectPtr = IObject*;
    using EntityPtr = IEntity*;
    using ComponentPtr = IComponent*;
    
    struct TypeInfo { 
        TypeInfo(World *pWorld, size_t id);

        bool operator==(const TypeInfo& other) const;

        size_t getId() const { return id; }

    private:
        size_t id;

#if SM_DEBUG
        World *pWorld;
#endif
    };

    struct ObjectData {
        TypeInfo info; // type id of this type

        Index index; // type id of this instance
        std::string name;
        World *pWorld;
    };
}

template<>
struct std::hash<game::TypeInfo> {
    size_t operator()(const game::TypeInfo& info) const {
        return info.getId();
    }
};
