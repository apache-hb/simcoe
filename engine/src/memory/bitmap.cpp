#include "engine/memory/bitmap.h"

using namespace engine;

// BitMap

bool BitMap::testSet(size_t index) {
    if (test(Index(index))) {
        return false;
    }

    set(index);
    return true;
}

// AtomicBitMap

bool AtomicBitMap::testSet(size_t index) {
    auto mask = getMask(index);

    return !(pBits[getWord(index)].fetch_or(mask) & mask);
}
