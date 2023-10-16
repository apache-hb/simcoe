#pragma once

#include "editor/game/level.h"

namespace editor {
    using namespace simcoe::math;

    struct SwarmGame final : GameLevel {
        SwarmGame();

        void update(float delta);

        float3 getWorldPos(float x, float y, float index = 0) {
            return kWorldOrigin + float3::from(index, x - 0.5f, y - 0.5f);
        }

        float3 getWorldScale() const {
            return kWorldScale;
        }

        float2 getWorldLimits() const {
            return { float(kWidth - 1), float(kHeight) };
        }

        float2 getAlienSpawn() const {
            return { 0.f, float(kHeight - 1) };
        }

        float2 getPlayerSpawn() const {
            return { 0.f, float(kHeight - 2) };
        }

        size_t getWidth() const { return kWidth; }
        size_t getHeight() const { return kHeight; }

    private:
        // game world info
        constexpr static size_t kWidth = 22;
        constexpr static size_t kHeight = 19;

        constexpr static float3 kWorldScale = float3::from(1.f, 1.f, 1.f) * float3::from(0.5f);
        constexpr static float3 kWorldOrigin = float3::from(0.f, 0.f, 0.f);

        // gameplay info
        constexpr static float kAlienSpeed = 2.f;
        constexpr static float kPlayerSpeed = 4.f;

        // game objects
        IGameObject *pPlayerObject = nullptr;
        IGameObject *pAlienObject = nullptr;
    };
}
