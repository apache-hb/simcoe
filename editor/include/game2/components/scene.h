#pragma once

#include "engine/math/math.h"

#include "game2/object.h"

namespace game {
    using namespace simcoe::math;

    struct Transform {
        float3 position = float3::zero();
        float3 rotation = float3::zero();
        float3 scale = float3::unit();

        void applyLocalTransform(const Transform& transform);
    };

    struct ISceneComponent : IComponent {
        ISceneComponent(ISceneComponent *pParent)
            : IComponent(pParent->getParent())
            , pParentComponent(pParent)
        { }

        virtual Transform getLocalTransform() const = 0;
        virtual Transform getWorldTransform() const;

        float3 getPosition() const;
        float3 getRotation() const;
        float3 getScale() const;

        float3 getWorldPosition() const;
        float3 getWorldRotation() const;
        float3 getWorldScale() const;

        ISceneComponent *getParentComponent() const { return pParentComponent; }

    protected:
        void setParentComponent(ISceneComponent *pParent) { pParentComponent = pParent; }

    private:
        ISceneComponent *pParentComponent = nullptr;
    };

    struct RootSceneComponent : ISceneComponent {
        void setPosition(const float3& newPosition);
        void setRotation(const float3& newRotation);
        void setScale(const float3& newScale);

        Transform getLocalTransform() const override { return transform; }
        Transform getWorldTransform() const override { return transform; }

    private:
        Transform transform;
    };
}
