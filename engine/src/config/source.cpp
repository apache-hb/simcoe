#include "engine/config/source.h"

using namespace simcoe;
using namespace simcoe::config;

bool config::isArrayAll(const NodeVec& nodes, NodeType type) {
    for (auto pNode : nodes) {
        if (pNode->getType() != type) {
            return false;
        }
    }

    return true;
}
