#pragma once

#include "engine/math/math.h"

#include <unordered_map>

namespace simcoe::math {
    template<typename T>
    struct SparseMatrix {
        bool isEmpty(int2 pos) const {
            return data.find(pos) == data.end();
        }

        T &at(int2 pos) {
            return data[pos];
        }

    private:
        std::unordered_map<int2, T> data;
    };
}
