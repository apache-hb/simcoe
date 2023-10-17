#pragma once

#include "editor/game/level.h"
#include "editor/game/input.h"
#include <random>

namespace editor::game {
    using namespace simcoe::math;

    struct OAlien : IGameObject {
        OAlien(GameLevel *pLevel, std::string name);

        void tick(float delta) override;

    private:
        // config
        float speed = 2.f;
        float eggSpawnRate = 2.f;
        size_t seed = 100;

        // egg spawning logic
        void trySpawnEgg();
        void spawnEgg();
        bool canSpawnEgg() const;

        float lastEggSpawn = 0.f;
        std::mt19937 rng;
        std::uniform_real_distribution<float> dist;
    };

    struct OBullet : IGameObject {
        OBullet(GameLevel *pLevel, IGameObject *pParent, float2 velocity);

        void tick(float delta) override;

        bool isParent(IGameObject *pObject) const;

    private:
        // bullet logic
        IGameObject *pParent; // parent object (we cant hit the parent)
        float2 velocity; // velocity vector
    };

    struct OLife : IGameObject {
        OLife(GameLevel *pLevel, size_t life);
    };

    struct OPlayer : IGameObject {
        OPlayer(GameLevel *pLevel, std::string name);

        void tick(float delta) override;

    private:
        // config
        float speed = 5.f;
        float bulletSpeed = 10.f;
        size_t initialLives = 3;



        // shooting logic
        void tryShootBullet(float angle);

        float lastFire = 0.f;
        float fireRate = 0.3f;



        // life handling logic
        void createLives();
        void addLife();
        void removeLife();

        size_t maxLives = 5;
        size_t currentLives = 0;
        std::vector<OLife*> lifeObjects;
    };

    struct OEgg : IGameObject {
        OEgg(GameLevel *pLevel, std::string name);

        void tick(float delta) override;

    private:
        // config
        float bulletSpeed = 7.f;

        float timeToMedium = 1.5f;
        float timeToLarge = 3.f;
        float timeToHatch = 5.f;

        // hatching logic
        void updateEggStage();
        float2 getShootVector(IGameObject *pTarget) const;

        float timeAlive = 0.f;
    };

    struct OGrid : IGameObject {
        OGrid(GameLevel *pLevel, std::string name);
    };

    struct SwarmGameInfo {
        // graphics config
        render::IMeshBufferHandle *pAlienMesh = nullptr;
        render::IMeshBufferHandle *pPlayerMesh = nullptr;
        render::IMeshBufferHandle *pBulletMesh = nullptr;
        render::IMeshBufferHandle *pGridMesh = nullptr;

        render::IMeshBufferHandle *pEggSmallMesh = nullptr;
        render::IMeshBufferHandle *pEggMediumMesh = nullptr;
        render::IMeshBufferHandle *pEggLargeMesh = nullptr;

        size_t alienTextureId = SIZE_MAX;
        size_t playerTextureId = SIZE_MAX;
        size_t bulletTextureId = SIZE_MAX;
        size_t gridTextureId = SIZE_MAX;

        size_t eggSmallTextureId = SIZE_MAX;
        size_t eggMediumTextureId = SIZE_MAX;
        size_t eggLargeTextureId = SIZE_MAX;

        // input config
        GameInputClient *pInputClient = nullptr;
    };

    struct SwarmGame : GameLevel {
        SwarmGame() { }

        void create(const SwarmGameInfo& info);
        void tick();

        template<typename T, typename... A>
        T *newObject(A&&... args) {
            T *pObject = GameLevel::addObject<T>(args...);
            pObject->scale *= getWorldScale();
            return pObject;
        }

        // getters
        const SwarmGameInfo& getInfo() const { return info; }

        float2 getAlienSpawnPoint() const { return float2::from(0.f, float(height - 1)); }
        float2 getPlayerSpawnPoint() const { return float2::from(0.f, float(height - 2)); }

        float3 getWorldScale() const { return worldScale; }
        float2 getWorldLimits() const { return float2::from(float(width - 1), float(height)); }

        float3 getWorldPos(float x, float y, float z = 0.f) const {
            return float3::from(z, x, y) + worldOrigin;
        }

        size_t getWidth() const { return width; }
        size_t getHeight() const { return height; }

        OAlien *getAlien() const { return pAlien; }
        OPlayer *getPlayer() const { return pPlayer; }

    private:
        // config
        size_t width = 22;
        size_t height = 19;

        SwarmGameInfo info;
        float3 worldScale = float3::from(0.5f);
        float3 worldOrigin = float3::zero();

        // game state
        OAlien *pAlien = nullptr;
        OPlayer *pPlayer = nullptr;
        OGrid *pGrid = nullptr;

        // time management
        float lastTick = 0.f;

        bool shouldCullObject(IGameObject *pObject) const;
    };
}
