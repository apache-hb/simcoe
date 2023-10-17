#pragma once

#include "editor/game/object.h"

#include <unordered_set>

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

    struct GameLevel;

    struct IGameObject {
        IGameObject(GameLevel *pLevel, std::string name)
            : pLevel(pLevel)
            , name(name)
        { }

        virtual ~IGameObject() = default;

        math::float3 position = { 0.0f, 0.0f, 0.0f };
        math::float3 rotation = { 0.0f, 0.0f, 0.0f }; // rotate around z-axis
        math::float3 scale = { 1.f, 1.f, 1.f };

        std::string_view getName() const { return name; }
        render::IMeshBufferHandle *getMesh() const { return pMesh; }
        size_t getTexture() const { return textureId; }

        virtual void tick(float delta) { }

    protected:
        void setTextureId(size_t id) { textureId = id; }
        void setMesh(render::IMeshBufferHandle *pNewMesh) { pMesh = pNewMesh; }

        GameLevel *pLevel;

    private:
        std::string name;

        render::IMeshBufferHandle *pMesh = nullptr;
        size_t textureId = SIZE_MAX;
    };

    struct GameLevel {
        math::float3 cameraPosition = { -10.0f, 0.0f, 0.0f };
        math::float3 cameraRotation = { 1.0f, 0.0f, 0.0f };
        float fov = 90.f;

        IProjection *pProjection = nullptr;

        template<typename T, typename... A>
        T *addObject(A&&... args) {
            static_assert(std::is_base_of_v<IGameObject, T>);

            T *pObject = new T(this, args...);

            std::lock_guard guard(lock);
            objects.push_back(pObject);
            return pObject;
        }

        void removeObject(IGameObject *pObject) {
            std::lock_guard guard(lock);
            std::erase(objects, pObject);
        }

        template<typename F>
        void useEachObject(F&& func) {
            std::lock_guard guard(lock);
            for (auto& pObject : objects)
                func(pObject);
        }

        template<typename F>
        void useObjects(F&& func) {
            std::lock_guard guard(lock);
            func(objects);
        }

        void deleteObject(IGameObject *pObject) {
            std::lock_guard guard(lock);
            std::erase(objects, pObject);

            delete pObject;
        }

    private:
        std::vector<IGameObject*> objects;
        std::recursive_mutex lock;
    };
}
