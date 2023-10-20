#pragma once

#include "engine/math/math.h"

#include <unordered_map>

namespace simcoe::math {
    template<typename TIndex, typename TData>
    struct SparseMatrix {
        bool isEmpty(TIndex pos) const {
            return data.find(pos) == data.end();
        }

        TData &at(TIndex pos) { return data[pos]; }
        const TData &at(TIndex pos) const { return data.at(pos); }

        void evict(TIndex pos) {
            data.erase(data.find(pos));
        }

    private:
        std::unordered_map<TIndex, TData> data;
    };
}
