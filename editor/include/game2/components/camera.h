#pragma once

#include "game2/components/scene.h"

namespace game {
    struct IView {
        virtual ~IView() = default;

        virtual float4x4 getViewMatrix() const = 0;
    };

    struct IProjection {
        virtual ~IProjection() = default;

        virtual float4x4 getProjectionMatrix() const = 0;

    protected:
        float nearPlane = 0.1f;
        float farPlane = 1000.f;
    };

    struct ICameraComponent : ISceneComponent {
        ICameraComponent(ISceneComponent *pParent)
            : ISceneComponent(pParent)
        { }

        virtual float4x4 getViewMatrix() const = 0;
        virtual float4x4 getProjectionMatrix() const = 0;
    };
}
