#pragma once

#include "engine/core/macros.h"

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

    namespace fs = assets::fs;

    struct GameLevel;
    struct IEntity;
    struct IEntityComponent;

    struct EntityCreateInfo {
        std::string name = "";
        GameLevel *pLevel = nullptr;
    };

    struct ComponentCreateInfo {
        std::string name = "";

        IEntity *pParent = nullptr;
        IEntityComponent *pParentComponent = nullptr;
    };

    struct IEntityComponent {
        IEntityComponent(const ComponentCreateInfo& info);

    private:
        IEntity *pParent;
        IEntityComponent *pParentComponent;
    };

    struct IEntity {
        IEntity(GameLevel *pLevel, std::string name, size_t id = SIZE_MAX);

        IEntity(const EntityCreateInfo& info);

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
}
