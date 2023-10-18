#pragma once

#include "editor/game/object.h"
#include "engine/os/system.h"

#include <unordered_set>

namespace editor::game {
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

        bool canCull() const { return bShouldCull; }

    protected:
        void setTextureId(size_t id) { textureId = id; }
        void setMesh(render::IMeshBufferHandle *pNewMesh) { pMesh = pNewMesh; }
        void setShouldCull(bool bShould) { bShouldCull = bShould; }

        GameLevel *pLevel;

    private:
        std::string name;
        bool bShouldCull = true;

        render::IMeshBufferHandle *pMesh = nullptr;
        size_t textureId = SIZE_MAX;
    };

    struct GameLevel {
        math::float3 cameraPosition = { -10.0f, 0.0f, 0.0f };
        math::float3 cameraRotation = { 1.0f, 0.0f, 0.0f };
        float fov = 90.f;

        IProjection *pProjection = nullptr;

        template<std::derived_from<IGameObject> T, typename... A> requires std::is_constructible_v<T, GameLevel*, A...>
        T *addObject(A&&... args) {
            T *pObject = new T(this, args...);

            std::lock_guard guard(lock);
            pending.emplace(pObject);
            return pObject;
        }

        void removeObject(IGameObject *pObject) {
            std::lock_guard guard(lock);
            std::erase(objects, pObject);
        }

        template<typename F> requires std::invocable<F, IGameObject*>
        void useEachObject(F&& func) {
            std::lock_guard guard(lock);
            for (auto& pObject : objects)
                func(pObject);
        }

        template<typename F> requires std::invocable<F, std::span<IGameObject*>>
        void useObjects(F&& func) {
            std::lock_guard guard(lock);
            func(objects);
        }

        void deleteObject(IGameObject *pObject);

        void beginTick();
        void endTick();

        float getCurrentTime() const { return clock.now(); }

    private:
        Clock clock;

        // object management
        std::unordered_set<IGameObject*> pending;
        std::unordered_set<IGameObject*> retired;

        std::vector<IGameObject*> objects;
        std::recursive_mutex lock;
    };
}
