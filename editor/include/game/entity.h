#pragma once

#include "engine/core/macros.h"

#include "editor/debug/debug.h"
#include "editor/graph/assets.h"
#include "editor/graph/mesh.h"

#include "engine/service/platform.h"
#include "engine/service/logging.h"

#include "imgui/imgui.h"

#include <unordered_set>

namespace game {
    using namespace simcoe;
    using namespace simcoe::math;

    using namespace editor;

    namespace fs = assets::fs;

    struct World;
    struct Level;
    struct IEntity;
    struct IEntityComponent;

    struct EntityInfo {
        std::string name = "";

        size_t type = SIZE_MAX;
        size_t version = SIZE_MAX;

        Level *pLevel = nullptr;
        World *pWorld = nullptr;
    };

    struct IEntity {
        IEntity(Level *pLevel, std::string name, size_t id = SIZE_MAX);

        IEntity(const EntityInfo& info);

        virtual ~IEntity() = default;

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

        Level *pLevel;

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
}
