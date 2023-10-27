#pragma once

#include <vector>
#include <string>

namespace game {
    struct IComponent;
    struct IObject;

    struct Level;

    /**
     * @brief a component attached to an object
     */
    struct IComponent {
        virtual ~IComponent() = default;

        IComponent(IObject *pObject)
            : pObject(pObject)
        { }

        void acceptTick(float delta) { tick(delta); }

        // getters
        IObject *getParent() const { return pObject; }

    protected:
        virtual void tick(float delta) { }

        // setters
        void setParent(IObject *pNewObject) {
            pObject = pNewObject;
        }

    private:
        IObject *pObject = nullptr;
    };

    struct ObjectCreateInfo {
        std::string name = "";
        Level *pLevel = nullptr;
    };

    /**
     * @brief any game object that can tick
     */
    struct IObject {
        virtual ~IObject() = default;
        IObject(const ObjectCreateInfo& info);

        void acceptTick(float delta) {
            for (auto *pComponent : components) {
                pComponent->acceptTick(delta);
            }

            tick(delta);
        }

    protected:
        virtual void tick(float delta) { }

        template<typename T, typename... A>
        T *newComponent(A&&... args) {
            T *pComponent = new T(this, std::forward<A>(args)...);
            components.push_back(pComponent);
            return pComponent;
        }

    private:
        std::vector<IComponent*> components;
    };
}