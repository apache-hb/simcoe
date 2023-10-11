#pragma once

#include "editor/game/object.h"

namespace editor {
    ///
    /// view matrices
    ///

    struct ICamera {
        virtual math::float4x4 getViewMatrix() const = 0;
    };

    struct TrackingCamera final : ICamera {
        TrackingCamera(math::float3 eye, math::float3 target, math::float3 up)
            : eye(eye)
            , target(target)
            , up(up)
        { }

        math::float4x4 getViewMatrix() const override {
            return math::float4x4::lookAtRH(eye, target, up);
        }

        math::float3 eye;
        math::float3 target;
        math::float3 up;
    };

    struct FreeCamera final : ICamera {
        FreeCamera(math::float3 eye, math::float3 direction, math::float3 up)
            : eye(eye)
            , direction(direction)
            , up(up)
        { }

        math::float4x4 getViewMatrix() const override {
            return math::float4x4::lookToRH(eye, direction, up);
        }

        math::float3 eye;
        math::float3 direction;
        math::float3 up;
    };

    ///
    /// projection matrices
    ///

    struct IProjection {
        virtual math::float4x4 getProjectionMatrix(float aspectRatio, float fov) const = 0;
    };

    struct Perspective final : IProjection {
        math::float4x4 getProjectionMatrix(float aspectRatio, float fov) const override {
            return math::float4x4::perspectiveRH(fov * math::kDegToRad<float>, aspectRatio, 0.1f, 1000.f);
        }
    };

    struct Orthographic final : IProjection {
        Orthographic(float width, float height)
            : width(width)
            , height(height)
        { }

        math::float4x4 getProjectionMatrix(float aspectRatio, float fov) const override {
            return math::float4x4::orthographicRH(width * aspectRatio, height, 0.1f, 125.f);
        }

    private:
        float width;
        float height;
    };

    ///
    /// game level
    ///

    struct GameObject {
        std::string name;

        math::float3 position = { 0.0f, 0.0f, 0.0f };
        math::float3 rotation = { 0.0f, 0.0f, 0.0f }; // rotate around z-axis
        math::float3 scale = { 1.f, 1.f, 1.f };
    };

    struct GameLevel {
        std::vector<GameObject> objects;

        math::float3 cameraPosition = { 5.0f, 5.0f, 5.0f };
        math::float3 cameraRotation = { 1.0f, 0.0f, 0.0f };
        float fov = 90.f;

        IProjection *pProjection = nullptr;
    };
}
