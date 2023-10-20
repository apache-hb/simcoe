#include "engine/physics/world.h"

using namespace simcoe;
using namespace simcoe::physics;
using namespace simcoe::math;

namespace {
    bool bodiesOverlap(const RigidBody *pBody1, const RigidBody *pBody2) {
        auto pos1 = pBody1->position;
        auto pos2 = pBody2->position;

        auto size1 = pBody1->size;
        auto size2 = pBody2->size;

        auto min1 = pos1 - size1;
        auto min2 = pos2 - size2;

        auto max1 = pos1 + size1;
        auto max2 = pos2 + size2;

        return min1.x <= max2.x && min2.x <= max1.x
            && min1.y <= max2.y && min2.y <= max1.y;
    }
}

World2D::World2D(float cellSize)
    : cellSize(cellSize)
{ }

void World2D::tick(float delta) {
    for (RigidBody *pBody : bodies) {
        for (int2 cell : cells[pBody]) {
            if (!bodyOverlapsOthers(pBody, cell)) {
                continue;
            }
        }

        pBody->position += pBody->velocity * delta;
        updateBody(pBody);
    }
}

bool World2D::bodyOverlapsOthers(const RigidBody *pBody, int2 cell) const {
    for (const RigidBody *pOther : grid.at(cell)) {
        if (pBody == pOther) {
            continue;
        }

        if (bodiesOverlap(pBody, pOther)) {
            return true;
        }
    }

    return false;
}

void World2D::addBody(RigidBody *pBody) {
    bodies.push_back(pBody);
    updateBody(pBody);
}

void World2D::updateBody(RigidBody *pBody) {
    cells.erase(pBody);

    auto pos = pBody->position;
    auto size = pBody->size;

    int2 min = ((pos - size).as<float>() + 0.5f).floor<int>();
    int2 max = ((pos + size).as<float>() - 0.5f).ceil<int>();

    for (int x = min.x; x <= max.x; ++x) {
        for (int y = min.y; y <= max.y; ++y) {
            grid.at(int2(x, y)).insert(pBody);
            cells[pBody].insert(int2(x, y));
        }
    }
}
