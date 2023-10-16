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

    struct Transform {
        math::float3 position;
        math::float3 rotation;
        math::float3 scale;
    };

    struct IGameObject {
        IGameObject(std::string name)
            : name(name)
        { }

        std::string name;
        size_t meshIndex = SIZE_MAX;

        math::float3 position = { 0.0f, 0.0f, 0.0f };
        math::float3 rotation = { 0.0f, 0.0f, 0.0f }; // rotate around z-axis
        math::float3 scale = { 1.f, 1.f, 1.f };
    };

    struct EnemyObject : IGameObject {
        EnemyObject(std::string name)
            : IGameObject(name)
        { }

        size_t health = 3;
        size_t maxHealth = 5;
    };

    struct PlayerObject : IGameObject {
        size_t lives = 3;
        size_t maxLives = 5;
    };

    struct GameLevel {
        math::float3 cameraPosition = { 5.0f, 5.0f, 5.0f };
        math::float3 cameraRotation = { 1.0f, 0.0f, 0.0f };
        float fov = 90.f;

        IProjection *pProjection = nullptr;

        void addObject(IGameObject *pObject) {
            std::lock_guard guard(lock);
            objects.push_back(pObject);
        }

        template<typename F>
        void useEachObject(F&& func) {
            std::lock_guard guard(lock);
            for (auto *pObject : objects)
                func(pObject);
        }

        template<typename F>
        void useObjects(F&& func) {
            std::lock_guard guard(lock);
            func(objects);
        }

    private:
        std::vector<IGameObject*> objects;
        std::mutex lock;
    };
}
