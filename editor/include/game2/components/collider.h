#pragma once

#include "engine/math/math.h"

#include "game2/components/scene.h"

namespace game {
    using namespace simcoe::math;

    struct RayCast {
        float3 origin; // world space
        float3 direction; // world space
        float distance = 0.f;
    };

    struct ICollider : ISceneComponent {
        ICollider(ISceneComponent *pParent)
            : ISceneComponent(pParent)
        { }

        /**
         * @brief check if ray intersects with this collider
         *
         * @param cast ray cast info
         * @return float distance to intersection point, or FLT_MAX if no intersection
         */
        virtual float rayIntersects(const RayCast& cast) const = 0;

    protected:
        float3 getColliderCenter() const;
    };

    // cube collider

    struct CubeBounds {
        float3 min;
        float3 max;
    };

    struct CubeCollider : ICollider {
        float rayIntersects(const RayCast& cast) const override;

        void setBounds(const CubeBounds& bounds);

    private:
        CubeBounds getColliderBounds() const;

        CubeBounds bounds;
    };

    // sphere collider

    struct SphereCollider : ICollider {
        float rayIntersects(const RayCast& cast) const override;

        void setRadius(float radius);

    private:
        float radius;
    };
}
