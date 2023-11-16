#pragma once

#include "engine/core/macros.h"
#include "engine/core/panic.h"

#include "game/render/scene.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace game {
    struct World;

    struct TypeInfo { 
        World *pWorld;
        size_t id;

        bool operator==(const TypeInfo& other) const {
            SM_ASSERTF(pWorld == other.pWorld, "comparing ids {}, {} from different worlds {} and {}", id, other.id, (void*)pWorld, (void*)other.pWorld);

            return id == other.id;
        }
    };

    using TypeTag = const TypeInfo&;
}

template<>
struct std::hash<game::TypeInfo> {
    size_t operator()(const game::TypeInfo& info) const {
        return info.id;
    }
};

namespace game {
    using ScenePass = render::ScenePass;

    struct IObject;
    struct IEntity;
    struct IComponent;
    struct IScene;
    struct World;

    using TypeTag = const TypeInfo&;

    size_t getUniqueId();
    TypeTag makeNameInfo(World *pWorld, const std::string& name);

    template<typename T>
    TypeTag makeTypeInfo(World *pWorld) {
        static TypeInfo info = { pWorld, getUniqueId() };
        return info;
    }

    struct ObjectData {
        TypeTag tag;
        std::string name;
    };

    struct IObject {
        virtual ~IObject() = default;

        IObject(ObjectData data) : data(data) { }

        TypeTag getTypeInfo() const { return data.tag; }

        const std::string& getName() const { return data.name; }
        World *getWorld() const { return data.tag.pWorld; }

    private:
        ObjectData data;
    };

    // entities

    struct IEntity : IObject {
        virtual ~IEntity() = default;

        IEntity(ObjectData info) : IObject(info) {}

        virtual void update(SM_UNUSED float delta) { }

        void addComponent(IComponent *pComponent);

        template<typename T>
        T *get() {
            auto expectedType = makeTypeInfo<T>(getWorld());

            if (auto it = components.find(expectedType); it != components.end()) {
                return static_cast<T*>(it->second);
            }

            return nullptr;
        }

    private:
        std::unordered_map<TypeInfo, IComponent*> components;
    };

    // components

    struct IComponent : IObject {
        virtual ~IComponent() = default;

        IComponent(ObjectData info) : IObject(info) { }
    };

    struct IScene {
        virtual ~IScene() = default;
    };

    template<typename T>
    struct EntityBuilder;

    struct World {
        template<typename T, typename... A> 
            requires std::derived_from<T, IEntity> 
                  && std::constructible_from<T, ObjectData, A...>
        EntityBuilder<T> create(std::string name, A&&... args) {
            auto info = makeTypeInfo<T>(this);

            SM_ASSERTF(entities.find(info) == entities.end(), "entity {} already exists", name);

            T *pEntity = new T({ info, name }, std::forward<A>(args)...);
            entities.emplace(info, pEntity);
            return pEntity;
        }

        template<typename T> requires std::derived_from<T, IEntity>
        T *get() {
            auto expectedType = makeTypeInfo<T>(this);

            if (auto it = entities.find(expectedType); it != entities.end()) {
                IEntity *pEntity = it->second;
                auto actualType = pEntity->getTypeInfo();
                SM_ASSERTF(actualType == expectedType, "entity {} is not of type {}, got {}", pEntity->getName(), expectedType.id, actualType.id);
                return static_cast<T*>(pEntity);
            }

            return nullptr;
        }

        // iterates over all entities of type T
        template<typename T, typename F>
        void each(F&& func) {
            auto expectedType = makeTypeInfo<T>();

            for (auto& [name, pEntity] : entities) {
                auto actualType = pEntity->getTypeInfo();
                if (actualType == expectedType) {
                    func(static_cast<T*>(pEntity));
                }
            }
        }

    private:
        std::unordered_map<TypeInfo, IEntity*> entities;
    };

    template<typename T>
    struct EntityBuilder {
        EntityBuilder(T *pEntity) 
            : pEntity(pEntity) 
        { }

        template<typename C, typename... A>
            requires std::derived_from<C, IComponent>
                  && std::constructible_from<C, ObjectData, A...>
        EntityBuilder<T>& add(A&&... args) {
            auto info = makeTypeInfo<T>(pEntity->getWorld());

            C *pComponent = new C({ info }, std::forward<A>(args)...);
            pEntity->addComponent(pComponent);
            return *this;
        }

        operator T*() { return pEntity; }

        T *pEntity = nullptr;
    };
}
