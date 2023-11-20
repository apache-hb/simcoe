#pragma once

#include "game/ecs/storage.h"

#include <unordered_map>

namespace game {
    template<typename T>
    using TypeInfoMap = std::unordered_map<TypeInfo, T>;

    using ComponentMap = TypeInfoMap<ComponentPtr>;

    struct IObject {
        virtual ~IObject() = default;

        IObject(ObjectData data) : data(data) { }

        TypeInfo getTypeInfo() const { return data.info; }

        size_t getTypeId() const { return data.info.getId(); }
        Index getInstanceId() const { return data.index; }

        const std::string& getName() const { return data.name; }
        World *getWorld() const { return data.pWorld; }

        virtual void onDestroy() { }

        virtual void onDebugDraw() { }

    private:
        ObjectData data;
    };

    // components

    struct ComponentData : ObjectData {
        
    };

    struct IComponent : IObject {
        virtual ~IComponent() = default;

        IComponent(ComponentData info) 
            : IObject(info)
        { }

        virtual void onCreate() { }

        void associate(IComponent *pComponent) {
            pAssociated = pComponent;
        }

        template<typename C>
        C *associated() const {
            auto expectedType = makeTypeInfo<C>(getWorld());
            auto realType = pAssociated->getTypeInfo();
            SM_ASSERTF(realType == expectedType, "component type mismatch");

            return static_cast<C*>(pAssociated);
        }

    private:
        IComponent *pAssociated = nullptr;
    };

    // entities

    struct EntityData : ObjectData {
        Index entityId;
    };

    struct IEntity : IObject {
        virtual ~IEntity() = default;

        IEntity(EntityData info) 
            : IObject(info)
            , entityId(info.entityId) 
        { }

        virtual void onCreate() { }

        void addComponent(IComponent *pComponent);

        template<typename T>
        T *get() const {
            TypeInfo expectedType = makeTypeInfo<T>(getWorld());

            if (auto it = components.find(expectedType); it != components.end()) {
                return static_cast<T*>(it->second);
            }

            return nullptr;
        }

        template<typename O>
        O *is() const {
            auto expectedType = makeTypeInfo<O>(getWorld());
            return expectedType == getTypeInfo() ? static_cast<O*>(this) : nullptr;
        }

        const ComponentMap& getComponents() const { return components; }

        Index getEntityId() const { return entityId; }

    private:
        Index entityId;
        ComponentMap components;
    };

}