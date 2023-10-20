#pragma once

#include "editor/debug/debug.h"
#include "editor/game/object.h"
#include "engine/os/system.h"

#include "imgui/imgui.h"

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
        template<typename F>
        IProjection(const char *name, F&& func)
            : debugHandle(std::make_unique<debug::DebugHandle>(name, [this, func] { debug(); func(); }))
        { }

        virtual math::float4x4 getProjectionMatrix(float aspectRatio) const = 0;
        debug::DebugHandle *getDebugHandle() { return debugHandle.get(); }

    protected:
        float nearLimit = 0.1f;
        float farLimit = 1000.f;

    private:
        void debug() {
            ImGui::SliderFloat("near", &nearLimit, 0.1f, 100.f);
            ImGui::SliderFloat("far", &farLimit, 0.1f, 1000.f);
        }

        debug::LocalHandle debugHandle;
    };

    struct Perspective final : IProjection {
        Perspective(float fov)
            : IProjection("Perspective", [this] { debug(); })
            , fov(fov)
        { }

        math::float4x4 getProjectionMatrix(float aspectRatio) const override {
            return math::float4x4::perspectiveRH(fov * math::kDegToRad<float>, aspectRatio, 0.1f, 1000.f);
        }

    private:
        float fov;

        void debug();
    };

    struct Orthographic final : IProjection {
        Orthographic(float width, float height)
            : IProjection("Orthographic", [this] { debug(); })
            , width(width)
            , height(height)
        { }

        math::float4x4 getProjectionMatrix(float aspectRatio) const override {
            return math::float4x4::orthographicRH(width * aspectRatio, height, 0.1f, 125.f);
        }

    private:
        float width;
        float height;

        void debug();
    };

    ///
    /// game level
    ///

    struct GameLevel;

    struct IGameObject {
        IGameObject(GameLevel *pLevel, std::string name)
            : pLevel(pLevel)
            , name(name)
            , debugHandle(std::make_unique<debug::DebugHandle>(name, [this] { debug(); }))
        { }

        template<typename F>
        IGameObject(GameLevel *pLevel, std::string name, F&& func)
            : pLevel(pLevel)
            , name(name)
            , debugHandle(std::make_unique<debug::DebugHandle>(name, [this, func] { debug(); func(); }))
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
        debug::DebugHandle *getDebugHandle() { return debugHandle.get(); }

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

        void debug();
        bool bLockScale = false;
        debug::LocalHandle debugHandle;
    };

    struct GameLevel {
        math::float3 cameraPosition = { -10.0f, 0.0f, 0.0f };
        math::float3 cameraRotation = { 1.0f, 0.0f, 0.0f };

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
