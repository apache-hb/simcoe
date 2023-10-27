#include "game2/components/scene.h"

using namespace game;

// Transform

void Transform::applyLocalTransform(const Transform& transform) {
    position += transform.position;
    rotation += transform.rotation;
    scale *= transform.scale;
}

// local transform

float3 ISceneComponent::getPosition() const {
    Transform transform = getLocalTransform();
    return transform.position;
}

float3 ISceneComponent::getRotation() const {
    Transform transform = getLocalTransform();
    return transform.rotation;
}

float3 ISceneComponent::getScale() const {
    Transform transform = getLocalTransform();
    return transform.scale;
}

// world transform

float3 ISceneComponent::getWorldPosition() const {
    Transform transform = getWorldTransform();
    return transform.position;
}

float3 ISceneComponent::getWorldRotation() const {
    Transform transform = getWorldTransform();
    return transform.rotation;
}

float3 ISceneComponent::getWorldScale() const {
    Transform transform = getWorldTransform();
    return transform.scale;
}

Transform ISceneComponent::getWorldTransform() const {
    if (ISceneComponent *pParent = getParentComponent()) {
        Transform trs = pParent->getWorldTransform();
        trs.applyLocalTransform(getLocalTransform());
        return trs;
    }

    return getLocalTransform();
}
