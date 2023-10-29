#pragma once

#include "engine/math/math.h"
#include "engine/core/slotmap.h"

// fwd

namespace simcoe::input {
    struct Manager;
}

namespace simcoe::render {
    struct Context;
    struct Graph;
}

namespace game {
    struct World;
    struct ILevel;
    struct IEntity;

    namespace graph {
        struct HudPass;
        struct ScenePass;
    }
}

// world

namespace game {
    using namespace simcoe;
    using namespace simcoe::math;

    // consts

    constexpr inline float3 kUpVector = float3::from(0.f, 0.f, 1.f); // z-up
    constexpr inline float3 kRightVector = float3::from(0.f, -1.f, 0.f); // left handed
    constexpr inline float3 kForwardVector = float3::from(1.f, 0.f, 0.f); // x-forward

    enum struct EntityVersion : size_t {
        eInvalid = SIZE_MAX
    };

    using EntitySlotMap = simcoe::SlotMap<EntityVersion, EntityVersion::eInvalid>;
    using EntitySlot = EntitySlotMap::Index;
    using EntitySlotType = std::underlying_type_t<EntitySlot>;
    using EntityVersionType = std::underlying_type_t<EntityVersion>;

    struct EntityTag {
        EntitySlot slot;
        EntityVersion version;
    };

    struct EntityInfo {
        std::string name;

        EntityTag tag;
        World *pWorld;
        ILevel *pLevel;
    };

    struct WorldInfo {
        // game config
        size_t entityLimit = 0x1000;
        size_t seed = 0;

        // input config
        input::Manager *pInput = nullptr;

        // render config
        render::Context *pRenderContext = nullptr;
        render::Graph *pRenderGraph = nullptr;
        size_t renderFaultLimit = 3;

        // game render config
        graph::HudPass *pHudPass = nullptr;
        graph::ScenePass *pScenePass = nullptr;

        void verify() const;
    };

    struct LevelInfo {
        size_t entityLimit = 0x1000;
        World *pWorld = nullptr;
    };
}

// std stuff

template<typename T>
struct std::formatter<game::EntityTag, T> : std::formatter<std::string_view, T> {
    template<typename FormatContext>
    auto format(const game::EntityTag& tag, FormatContext& ctx) {
        auto it = std::format("(index={}, version={})", game::EntitySlotType(tag.slot), game::EntityVersionType(tag.version));
        return std::formatter<std::string_view, T>::format(it, ctx);
    }
};

template<>
struct std::hash<game::EntityTag> {
    size_t operator()(const game::EntityTag& tag) const {
        return std::hash<game::EntitySlotType>()(game::EntitySlotType(tag.slot)) ^ std::hash<game::EntityVersionType>()(game::EntityVersionType(tag.version));
    }
};

template<>
struct std::equal_to<game::EntityTag> {
    bool operator()(const game::EntityTag& lhs, const game::EntityTag& rhs) const {
        return lhs.slot == rhs.slot && lhs.version == rhs.version;
    }
};
