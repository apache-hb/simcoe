#pragma once

#include "game/level.h"

#include "swarm/objects.h"
#include "swarm/input.h"

using namespace simcoe;
using namespace simcoe::math;

using namespace editor;

namespace swarm {
    struct MainMenu : game::GameLevel {

    };

    struct PlayLevel : game::GameLevel {
        enum Projection : int {
            ePerspective,
            eOrthographic
        };

        virtual ~PlayLevel() = default;

        PlayLevel();

        void tick(float delta) override;

        template<std::derived_from<swarm::OSwarmObject> T, typename... A>
        T *newObject(A&&... args) {
            T *pObject = GameLevel::addObject<T>(args...);

            if constexpr (!std::is_same_v<T, swarm::OBullet>)
                nonBulletObjects.push_back(pObject);

            pObject->scale *= getWorldScale();
            return pObject;
        }

        // getters
        float2 getAlienSpawnPoint() const { return float2::from(0.f, float(height - 1)); }
        float2 getPlayerSpawnPoint() const { return float2::from(0.f, float(height - 2)); }

        float3 getWorldScale() const { return worldScale; }
        float2 getWorldLimits() const { return float2::from(float(width - 1), float(height - 1)); }

        float3 getWorldPos(float x, float y, float z = 0.f) const {
            return float3::from(z, x, y) + worldOrigin;
        }

        size_t getWidth() const { return width; }
        size_t getHeight() const { return height; }

        OAlien *getAlien() const { return pAlien; }
        OPlayer *getPlayer() const { return pPlayer; }

    private:
        void debug() override;

        // config
        size_t width = 22;
        size_t height = 19;

        Projection currentProjection = eOrthographic;
        std::array<game::IProjection*, 2> projections = std::to_array({
            static_cast<game::IProjection*>(new game::Perspective(90.f)),
            static_cast<game::IProjection*>(new game::Orthographic(24.f, 24.f))
        });
        void setProjection(Projection projection) {
            currentProjection = projection;
            pProjection = projections[currentProjection];
        }

        util::TimeStep timeStepper = {1.f / 60.f};
        float3 worldScale = float3::from(0.5f);
        float3 worldOrigin = float3::zero();

        // game state
        swarm::OAlien *pAlien = nullptr;
        swarm::OPlayer *pPlayer = nullptr;
        swarm::OGrid *pGrid = nullptr;

    public:
        // how horrible
        std::vector<swarm::OSwarmObject*> nonBulletObjects;

        bool shouldCullObject(game::IEntity *pObject) const;
    };

    struct GameOverLevel : game::GameLevel {
        GameOverLevel();

        void tick(float delta) override;

    private:
        void debug() override;
    };
}
