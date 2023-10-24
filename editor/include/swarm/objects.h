#pragma once

#include "editor/debug/debug.h"
#include "editor/game/level.h"

#include "engine/util/time.h"

#include <random>

using namespace simcoe;
using namespace simcoe::math;

using namespace editor;

namespace swarm {
    enum ObjectType {
        eAlien,
        ePlayer,
        eEgg,
        eAggroAlien,
        eBullet,
        eLife,
        eGrid,
        eGameOver
    };

    struct OSwarmObject : game::IGameObject {
        OSwarmObject(game::GameLevel *pLevel, std::string name, ObjectType type)
            : game::IGameObject(pLevel, name, size_t(type))
        { }

        virtual void onHit() { }
    };

    struct OAlien : OSwarmObject {
        OAlien(game::GameLevel *pLevel, std::string name);

        void tick(float delta) override;

        void onHit() override { retire(); }

    private:
        // config
        float eggSpawnRate = 2.f;
        size_t seed = 100;

        ///////////////////////////////////
        /// movement logic
        ///////////////////////////////////
        void move();
        bool canMove() const;
        float moveRate = 0.7f; // tiles per second
        float lastMove = 0.f; // time since last move

        ///////////////////////////////////
        /// egg spawning logic
        ///////////////////////////////////
        void spawnEgg();
        bool canSpawnEgg() const;

        float lastEggSpawn = 0.f;
        std::mt19937 rng;
        std::uniform_real_distribution<float> dist;
    };

    struct OBullet : OSwarmObject {
        OBullet(game::GameLevel *pLevel, IGameObject *pParent, float2 velocity);

        void tick(float delta) override;

        bool canCollide(IGameObject *pOther) const;

    private:
        // bullet logic
        IGameObject *pParent; // parent object (we cant hit the parent)
        float2 velocity; // velocity vector
    };

    struct OLife : OSwarmObject {
        OLife(game::GameLevel *pLevel, size_t life);

        void onHit() override { }
    };

    struct OPlayer : OSwarmObject {
        OPlayer(game::GameLevel *pLevel, std::string name);

        void tick(float delta) override;

        void onHit() override;

    private:
        // config
        float speed = 5.f;
        float bulletSpeed = 10.f;
        size_t initialLives = 3;

        ///////////////////////////////////
        /// shooting logic
        ///////////////////////////////////
        void tryShootBullet(float angle);

        float lastFire = 0.f;
        float fireRate = 0.01f; // seconds between shots

        ///////////////////////////////////
        /// life handling logic
        ///////////////////////////////////
        void createLives();
        void addLife();
        void removeLife();

        bool isInvulnerable() const;

        float lastHit = 0.f;
        float invulnTime = 1.f;

        size_t maxLives = 5;
        size_t currentLives = 0;
        std::vector<OLife*> lifeObjects;

        void debug() override;
    };

    struct OEgg : OSwarmObject {
        OEgg(game::GameLevel *pLevel, std::string name);

        void tick(float delta) override;

        void onHit() override { pLevel->deleteObject(this); }

    private:
        // config
        float bulletSpeed = 7.f;

        float timeToMedium = 1.5f;
        float timeToLarge = 3.f;
        float timeToHatch = 5.f;

        float2 getShootVector(IGameObject *pTarget) const;

        ///////////////////////////////////
        /// hatching logic
        ///////////////////////////////////
        void updateEggStage();

        float timeAlive = 0.f;
    };

    struct OAggroAlien : OSwarmObject {
        OAggroAlien(game::GameLevel *pLevel, game::IGameObject *pParent);

        void tick(float delta) override;

        void onHit() override { pLevel->deleteObject(this); }

    private:
        game::IGameObject *pParent;

        ///////////////////////////////////
        /// hit logic
        ///////////////////////////////////
        void hitPlayer();

        ///////////////////////////////////
        /// movement logic
        ///////////////////////////////////
        void move();
        bool canMove() const;
        float moveRate = 0.1f;
        float lastMove = 0.f;

        bool bMovingUp = false;
        bool bMovingRight = false;
    };

    struct OGrid : OSwarmObject {
        OGrid(game::GameLevel *pLevel, std::string name);
    };

    struct OGameOver : game::IGameObject {
        OGameOver(game::GameLevel *pLevel, std::string name);

        void tick(float delta) override;
    };
}
