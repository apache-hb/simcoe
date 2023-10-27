#include "game2/components/collider.h"

using namespace game;

namespace {
    constexpr bool computeQuadratic(float a, float b, float c, float& x0, float& x1) {
        float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f) {
            return false;
        }

        float sqrtDiscriminant = sqrtf(discriminant);
        float q = (b > 0.0f) ? -0.5f * (b + sqrtDiscriminant) : -0.5f * (b - sqrtDiscriminant);
        x0 = q / a;
        x1 = c / q;
        return true;
    }
}

/// collider

ICollider::ICollider(ISceneComponent *pParentComponent)
    : ISceneComponent(pParentComponent)
{ }

ICollider::~ICollider() {

}

float3 ICollider::getColliderCenter() const {
    return getWorldPosition();
}

/// cube collider

CubeBounds CubeCollider::getColliderBounds() const {
    float3 position = getColliderCenter();

    CubeBounds cube;
    cube.min = bounds.min + position;
    cube.max = bounds.max + position;
    return cube;
}

float CubeCollider::rayIntersects(const RayCast& cast) const {
    ASSERTF(cast.distance > 0, "invalid distance");

    // TODO: handle rotation and scale
    ASSERTF(getWorldScale() == float3::unit(), "only unit scale is supported for now");
    ASSERTF(getWorldRotation() == float3::zero(), "only zero rotation is supported for now");

#if 0
    float3 worldRotation = getWorldRotation();

    float3 castOrigin = float3::rotate(cast.origin, getColliderCenter(), getWorldRotation());
    float3 castDirection = float3::rotate(cast.direction, float3::zero(), getWorldRotation());
#endif

    CubeBounds cube = getColliderBounds();
    float3 bounds[] = { cube.min, cube.max };

    bool sign0 = cast.direction.x < 0;
    bool sign1 = cast.direction.y < 0;

    float tmin = (bounds[sign0].x - cast.origin.x) / cast.direction.x;
    float tmax = (bounds[1 - sign0].x - cast.origin.x) / cast.direction.x;
    float tymin = (bounds[sign1].y - cast.origin.y) / cast.direction.y;
    float tymax = (bounds[1 - sign1].y - cast.origin.y) / cast.direction.y;

    if ((tmin > tymax) || (tymin > tmax))
        return FLT_MAX;

    if (tymin > tmin)
        tmin = tymin;

    if (tymax < tmax)
        tmax = tymax;

    bool sign2 = cast.direction.z < 0;

    float tzmin = (bounds[sign2].z - cast.origin.z) / cast.direction.z;
    float tzmax = (bounds[1 - sign2].z - cast.origin.z) / cast.direction.z;

    if ((tmin > tzmax) || (tzmin > tmax))
        return FLT_MAX;

    if (tzmin > tmin)
        tmin = tzmin;
    if (tzmax < tmax)
        tmax = tzmax;

    if (tmin < 0) {
        tmin = tmax;
        if (tmin < 0)
            return FLT_MAX;
    }

    return tmin;
}

// sphere collider

float SphereCollider::rayIntersects(const RayCast& cast) const {
    ASSERTF(radius > 0, "invalid radius");
    ASSERTF(cast.distance > 0, "invalid distance");

    // TODO: handle rotation and non-uniform scale
    ASSERTF(getWorldScale().isUniform(), "only unit scale is supported for now");
    ASSERTF(getWorldRotation() == float3::zero(), "only zero rotation is supported for now");

    float3 center = getColliderCenter();

    float3 l = cast.origin - center;
    float a = float3::dot(cast.direction, cast.direction);
    float b = 2.0f * float3::dot(cast.direction, l);
    float c = float3::dot(l, l) - radius * radius;

    float t0, t1;
    if (!computeQuadratic(a, b, c, t0, t1))
        return FLT_MAX;

    if (t0 > t1)
        std::swap(t0, t1);

    if (t0 < 0) {
        t0 = t1;
        if (t0 < 0)
            return FLT_MAX;
    }

    return t0;
}
