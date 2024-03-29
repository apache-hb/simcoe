#pragma once

#include "game/render/scene.h"

#include "game/ecs/typeinfo.h"
#include "game/ecs/storage.h"
#include "game/ecs/objects.h"

#include <ranges>

namespace game {
    size_t getUniqueId();
    TypeInfo makeNameInfo(World *pWorld, const std::string& name);

    template<typename T>
    TypeInfo makeTypeInfo(World *pWorld) {
        static TypeInfo info = { pWorld, getUniqueId() };
        return info;
    }

    using ObjectStorageMap = TypeInfoMap<ObjectStorage>;

    using EventFn = std::function<void(ObjectPtr)>;
    using AttachEventFn = std::function<void(EntityPtr, ComponentPtr)>;

    using EventMap = std::unordered_multimap<TypeInfo, EventFn>;
    using AttachEventMap = std::unordered_multimap<TypeInfo, AttachEventFn>;

    template<typename T>
    struct EntityBuilder;

    using FilterFn = std::function<bool(ObjectPtr)>;

    template<typename T>
    struct WorldStorage {
        struct Iterator {
            Iterator(StorageIter inIter, FilterFn inFilter) 
                : iter(inIter) 
                , filter(inFilter)
            { 
                while (iter && !filter(*iter)) {
                    ++iter;
                }

                SM_ASSERTF(!iter || filter(*iter), "iterator tried to return invalid object in constructor");
            }

            Iterator& operator++() { 
                // increment until filter returns true
                do {
                    ++iter;
                } while (iter && !filter(*iter));
                
                SM_ASSERTF(!iter || filter(*iter), "iterator tried to return invalid object");
                
                return *this; 
            }

            T *operator*() const { 
                SM_ASSERTF(iter, "iterator is invalid");

                auto i = *iter;
                SM_ASSERTF(filter(i), "iterator tried to return invalid object");

                return static_cast<T*>(i);
            }

            bool operator==(const Iterator& other) const { 
                return iter == other.iter; 
            }

        private:
            StorageIter iter;
            FilterFn filter;
        };

        WorldStorage(ObjectStorage *pStorage, FilterFn filter = [](ObjectPtr) { return true; }) 
            : pStorage(pStorage)
            , filter(filter)
        { }

        Iterator begin() { return Iterator(pStorage->begin(), filter); }
        Iterator end() { return Iterator(pStorage->end(), filter); }
        
    private:
        ObjectStorage *pStorage;
        FilterFn filter;
    };

    struct World {
        World() : entities(makeTypeInfo<IEntity>(this), 1024) { }

        // creation

        template<typename T = IEntity, typename... A> 
            requires std::derived_from<T, IEntity> 
                  && std::constructible_from<T, EntityData, A...>
        EntityBuilder<T> entity(std::string name, A&&... args) {
            TypeInfo info = makeTypeInfo<T>(this);

            ObjectData data = allocObject(info, name);
            Index entityId = entities.allocate();

            EntityData entityData = { data, entityId };
            T *pEntity = new T(entityData, std::forward<A>(args)...);
            insertObject(pEntity);

            insertEntity(entityId, pEntity);

            pEntity->onCreate();
            notifyCreate(pEntity);

            return pEntity;
        }

        template<typename T, typename... A>
            requires std::derived_from<T, IComponent>
                  && std::constructible_from<T, ComponentData, A...>
        T *component(A&&... args) {
            TypeInfo info = makeTypeInfo<T>(this);

            std::string name = "component";
            if constexpr (requires { T::kTypeName; }) {
                name = T::kTypeName;
            }

            ObjectData data = allocObject(info, name);
            ComponentData componentData = { data };
            T *pComponent = new T(componentData, std::forward<A>(args)...);
            insertObject(pComponent);

            pComponent->onCreate();
            notifyCreate(pComponent);

            return pComponent;
        }

        template<typename T> 
            requires std::derived_from<T, IEntity>
        T *get(Index id) {
            TypeInfo expectedType = makeTypeInfo<T>(this);

            if (auto it = objects.find(expectedType); it != objects.end()) {
                ObjectStorage& storage = it->second;
                ObjectPtr pObject = storage.get(id);
                SM_ASSERTF(pObject != nullptr, "object {} of type {} is null", size_t(id), expectedType.getId());

                TypeInfo actualType = pObject->getTypeInfo();
                SM_ASSERTF(actualType == expectedType, "object {} of type {} is not of type {}", size_t(id), actualType.getId(), expectedType.getId());

                return static_cast<T*>(pObject);
            }

            return nullptr;
        }

        void destroy(EntityPtr pEntity) {
            auto info = pEntity->getTypeInfo();

            notifyDestroy(pEntity);

            entities.release(pEntity->getEntityId());
            objects.at(info).release(pEntity->getInstanceId());

            notifyDestroy(pEntity);
            delete pEntity;
        }

        // events

        template<typename T, typename F>
        void onCreate(F&& func) {
            EventFn fn = [func](ObjectPtr ptr) {
                T *pOuter = static_cast<T*>(ptr);
                func(pOuter);
            };

            TypeInfo info = makeTypeInfo<T>(this);
            onCreateEvents.emplace(info, fn);
        }

        template<typename T, typename F>
        void onDestroy(F&& func) {
            EventFn fn = [func](ObjectPtr ptr) {
                T *pOuter = static_cast<T*>(ptr);
                func(pOuter);
            };

            TypeInfo info = makeTypeInfo<T>(this);
            onDestroyEvents.emplace(info, fn);
        }

        template<typename T, typename F>
        void onAttach(F&& func) {
            AttachEventFn fn = [func](EntityPtr pEntity, ComponentPtr pComponent) {
                T *pOuter = static_cast<T*>(pComponent);
                func(pEntity, pOuter);
            };

            TypeInfo info = makeTypeInfo<T>(this);
            onAttachEvents.emplace(info, fn);
        }

        // iteration

        WorldStorage<IEntity> all() {
            return WorldStorage<IEntity>(&entities);
        }

        template<typename T>
        WorldStorage<T> allOf() {
            TypeInfo expectedType = makeTypeInfo<T>(this);

            return WorldStorage<T>(&objects.at(expectedType));
        }

        template<typename... C>
            requires (std::derived_from<C, IComponent> && ...) 
        WorldStorage<IEntity> allWith() {
            auto filter = [](ObjectPtr pObject) {
                IEntity *pEntity = static_cast<IEntity*>(pObject);

                return ((pEntity->template get<C>()) && ...);
            };

            return WorldStorage<IEntity>(&entities, filter);
        }

        // iterates over all entities of type T
        template<typename T, typename F>
        void each(F&& func) {
            auto expectedType = makeTypeInfo<T>();

            each(expectedType, std::forward<F>(func));
        }

        void notifyAttach(EntityPtr pEntity, ComponentPtr pComponent) {
            auto info = pComponent->getTypeInfo();

            auto [front, back] = onAttachEvents.equal_range(info);
            for (auto it = front; it != back; ++it) {
                auto& fn = it->second;
                fn(pEntity, pComponent);
            }
        }

    private:
        void each(const TypeInfo& info, std::function<void(ObjectPtr)> func) {
            if (auto it = objects.find(info); it != objects.end()) {
                auto& storage = it->second;

                for (auto* pEntity : storage) {
                    func(pEntity);
                }
            }
        }

        ObjectData allocObject(const TypeInfo& info, const std::string& name) {
            if (objects.find(info) == objects.end()) {
                objects.emplace(info, ObjectStorage(info, 1024));
            }

            ObjectStorage& storage = objects.at(info);
            Index index = storage.allocate();
            return { info, index, name, this };
        }

        void insertObject(ObjectPtr pObject) {
            auto info = pObject->getTypeInfo();
            auto index = pObject->getInstanceId();

            if (objects.find(info) == objects.end()) {
                objects.emplace(info, ObjectStorage(info, 1024));
            }

            ObjectStorage& storage = objects.at(info);
            storage.insert(index, pObject);
        }

        void insertEntity(Index index, EntityPtr pEntity) {
            entities.insert(index, pEntity);
        }

        void notifyCreate(ObjectPtr pObject) {
            auto info = pObject->getTypeInfo();

            auto [front, back] = onCreateEvents.equal_range(info);
            for (auto it = front; it != back; ++it) {
                auto& fn = it->second;
                fn(pObject);
            }
        }

        void notifyDestroy(ObjectPtr pObject) {
            auto info = pObject->getTypeInfo();

            auto [front, back] = onDestroyEvents.equal_range(info);
            for (auto it = front; it != back; ++it) {
                auto& fn = it->second;
                fn(pObject);
            }
        }

        // TODO: make private
    public:
        ObjectStorage entities;
        ObjectStorageMap objects;

        EventMap onCreateEvents;
        EventMap onDestroyEvents;

        AttachEventMap onAttachEvents;
    };

    template<typename T>
    struct EntityBuilder {
        EntityBuilder(T *pEntity) 
            : pEntity(pEntity) 
        { }

        template<typename C, typename... A>
            requires std::derived_from<C, IComponent>
                  && std::constructible_from<C, ComponentData, A...>
        EntityBuilder<T>& add(A&&... args) {
            World *pWorld = pEntity->getWorld();
            C *pComp = pWorld->component<C>(std::forward<A>(args)...);
            pEntity->addComponent(pComp);
            return *this;
        }

        EntityBuilder<T>& add(ComponentPtr pComponent) {
            pEntity->addComponent(pComponent);
            return *this;
        }

        operator T*() { return pEntity; }

        T *pEntity = nullptr;
    };
}
