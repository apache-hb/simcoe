#pragma once

#include "editor/debug/debug.h"
#include "editor/graph/assets.h"
#include "editor/graph/mesh.h"

#include "engine/system/system.h"

#include "imgui/imgui.h"

#include <unordered_set>

namespace editor::game {
    using namespace simcoe;
    using namespace simcoe::math;

    namespace fs = assets::fs;

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

    struct GameLevel;

    struct IGameObject {
        IGameObject(GameLevel *pLevel, std::string name, size_t id = SIZE_MAX);

        virtual ~IGameObject() = default;

        float3 position = { 0.0f, 0.0f, 0.0f };
        float3 rotation = { 0.0f, 0.0f, 0.0f }; // rotate around z-axis
        float3 scale = { 1.f, 1.f, 1.f };

        std::string_view getName() const { return name; }
        size_t getId() const { return id; }

        render::IMeshBufferHandle *getMesh() const { return pMesh; }
        render::ResourceWrapper<graph::TextureHandle> *getTexture() const {
            return pTexture;
        }

        virtual void tick(float delta) { }
        virtual void debug() { }

        bool canCull() const { return bShouldCull; }
        debug::DebugHandle *getDebugHandle() { return debugHandle.get(); }
        void retire();

    protected:
        void setTexture(const fs::path& path);
        void setMesh(const fs::path& path);

        void setTextureHandle(render::ResourceWrapper<graph::TextureHandle> *pNewTexture) { pTexture = pNewTexture; }
        void setMeshHandle(render::IMeshBufferHandle *pNewMesh) { pMesh = pNewMesh; }

        void setShouldCull(bool bShould) { bShouldCull = bShould; }

        GameLevel *pLevel;

    private:
        size_t id = 0;
        std::string name;
        bool bShouldCull = true;

        fs::path currentTexture;
        fs::path currentMesh;

        std::atomic<render::ResourceWrapper<graph::TextureHandle>*> pTexture = nullptr;
        std::atomic<render::IMeshBufferHandle*> pMesh = nullptr;

        void objectDebug();
        bool bLockScale = false;
        debug::LocalHandle debugHandle;
    };

    struct GameLevel {
        GameLevel(std::string_view name)
            : name(name)
        { }

        float3 cameraPosition = { -10.0f, 0.0f, 0.0f };
        float3 cameraRotation = { 1.0f, 0.0f, 0.0f };

        IProjection *pProjection = nullptr;

        template<std::derived_from<IGameObject> T, typename... A>
            //requires std::is_constructible_v<T, GameLevel*, A...>
        T *addObject(A&&... args) {
            auto pObject = new T(this, args...);
            simcoe::logInfo("adding object: {}", (void*)pObject);

            std::lock_guard guard(lock);
            pending.emplace(pObject);
            return pObject;
        }

        void removeObject(IGameObject *pObject) {
            simcoe::logInfo("deleting object: {}", (void*)pObject);
            std::lock_guard guard(lock);
            std::erase(objects, pObject);
        }

        template<typename F> requires std::invocable<F, IGameObject*>
        void useEachObject(F&& func) {
            std::lock_guard guard(lock);
            for (auto pObject : objects)
                func(pObject);
        }

        template<typename F> requires std::invocable<F, std::span<IGameObject*>>
        void useObjects(F&& func) {
            std::lock_guard guard(lock);
            func(objects);
        }

        // only use this on the game thread pretty please
        std::span<IGameObject*> getObjects() { return objects; }

        void deleteObject(IGameObject *pObject);

        void beginTick();
        void endTick();

        float getCurrentTime() const { return clock.now(); }

        virtual void tick(float delta) { }
        virtual void pause() { simcoe::logInfo("pause"); }
        virtual void resume() { simcoe::logInfo("resume"); }

    private:
        system::Clock clock;

        // object management
        std::unordered_set<IGameObject*> pending;
        std::unordered_set<IGameObject*> retired;

        std::string_view name = "GameLevel";

    public:
        std::vector<IGameObject*> objects;
        std::recursive_mutex lock;

        std::string_view getName() const { return name; }
        virtual void debug();
    };
}
