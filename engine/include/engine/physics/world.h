#pragma once

#include "engine/math/math.h"
#include "engine/math/sparse.h"

#include <unordered_set>

namespace simcoe::physics {
    using namespace simcoe::math;

    struct RigidBody {
        float2 position;
        float2 size;
        float2 velocity;
    };

    struct World2D {
        World2D(float cellSize);

        void tick(float delta);

        void addBody(RigidBody *pBody);

    private:
        void updateBody(RigidBody *pBody);

        std::vector<RigidBody *> bodies;

        // the size of each cell in the grid
        float cellSize;

        // a sparse grid of cells, each cell contains a set of rigid bodies
        SparseMatrix<int2, std::unordered_set<const RigidBody*>> grid;

        // a map of each rigid body to the cells it currently overlaps
        std::unordered_map<const RigidBody*, std::unordered_set<int2>> cells;

        bool bodyOverlapsOthers(const RigidBody *pBody, int2 cell) const;
    };
}
