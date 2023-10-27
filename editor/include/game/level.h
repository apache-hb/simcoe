#pragma once

#include "game/entity.h"

#include "editor/debug/debug.h"
#include "editor/graph/assets.h"
#include "editor/graph/mesh.h"

#include "engine/service/platform.h"
#include "engine/service/logging.h"

#include "imgui/imgui.h"

#include <unordered_set>

namespace editor::game {
    using namespace simcoe;
    using namespace simcoe::math;

    ///
    /// view matrices
    ///

    struct ICamera {
        virtual float4x4 getViewMatrix() const = 0;
    };

    struct TrackingCamera final : ICamera {
        TrackingCamera(float3 eye, float3 target, float3 up)
            : eye(eye)
            , target(target)
            , up(up)
        { }

        float4x4 getViewMatrix() const override {
            return float4x4::lookAtRH(eye, target, up);
        }

        float3 eye;
        float3 target;
        float3 up;
    };

    struct FreeCamera final : ICamera {
        FreeCamera(float3 eye, float3 direction, float3 up)
            : eye(eye)
            , direction(direction)
            , up(up)
        { }

        float4x4 getViewMatrix() const override {
            return float4x4::lookToRH(eye, direction, up);
        }

        float3 eye;
        float3 direction;
        float3 up;
    };

    ///
    /// projection matrices
    ///

    struct IProjection {
        virtual ~IProjection() = default;

        template<typename F>
        IProjection(const char *name, F&& func)
            : debugHandle(std::make_unique<debug::DebugHandle>(name, [this, func] { debug(); func(); }))
        { }

        virtual float4x4 getProjectionMatrix(float aspectRatio) const = 0;
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

        float4x4 getProjectionMatrix(float aspectRatio) const override {
            return float4x4::perspectiveRH(fov * kDegToRad<float>, aspectRatio, 0.1f, 1000.f);
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

        float4x4 getProjectionMatrix(float aspectRatio) const override {
            return float4x4::orthographicRH(width * aspectRatio, height, 0.1f, 125.f);
        }

    private:
        float width;
        float height;

        void debug();
    };

    ///
    /// game level
    ///

    struct GameLevel {
        GameLevel(std::string_view name)
            : name(name)
        { }

        float3 cameraPosition = { -10.0f, 0.0f, 0.0f };
        float3 cameraRotation = { 1.0f, 0.0f, 0.0f };

        IProjection *pProjection = nullptr;

        template<std::derived_from<IEntity> T, typename... A>
            requires std::is_constructible_v<T, GameLevel*, A...>
        T *addObject(A&&... args) {
            auto pObject = new T(this, args...);
            LOG_INFO("adding object: {}", (void*)pObject);

            std::lock_guard guard(lock);
            pending.emplace(pObject);
            return pObject;
        }

        void removeObject(IEntity *pObject) {
            LOG_INFO("deleting object: {}", (void*)pObject);
            std::lock_guard guard(lock);
            std::erase(objects, pObject);
        }

        template<typename F> requires std::invocable<F, IEntity*>
        void useEachObject(F&& func) {
            std::lock_guard guard(lock);
            for (auto pObject : objects)
                func(pObject);
        }

        template<typename F> requires std::invocable<F, std::span<IEntity*>>
        void useObjects(F&& func) {
            std::lock_guard guard(lock);
            func(objects);
        }

        // only use this on the game thread pretty please
        std::span<IEntity*> getObjects() { return objects; }

        void deleteObject(IEntity *pObject);

        void beginTick();
        void endTick();

        float getCurrentTime() const { return clock.now(); }

        virtual void tick(float delta) { }
        virtual void pause() { LOG_INFO("pause"); }
        virtual void resume() { LOG_INFO("resume"); }

    private:
        Clock clock;

        // object management
        std::unordered_set<IEntity*> pending;
        std::unordered_set<IEntity*> retired;

        std::string_view name = "GameLevel";

    public:
        std::vector<IEntity*> objects;
        std::recursive_mutex lock;

        std::string_view getName() const { return name; }
        virtual void debug();
    };
}
