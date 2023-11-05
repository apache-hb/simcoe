#include "editor/ui/components/buffer.h"

#include "engine/core/units.h"

namespace core = simcoe::core;

using namespace editor;
using namespace editor::ui;

ScrollingBuffer::ScrollingBuffer(int size)
    : maxSize(size)
    , dataOffset(0)
{
    data.reserve(size);
}

void ScrollingBuffer::addPoint(float x, float y) {
    if (int(data.size()) < maxSize) {
        data.push_back(ImVec2(x, y));
    } else {
        data[dataOffset] = ImVec2(x, y);
        dataOffset = (dataOffset + 1) % maxSize;
    }
}

void ScrollingBuffer::reset() {
    if (!data.empty()) {
        data.clear();
        dataOffset = 0;
    }
}


int ScrollingBuffer::size() const { return core::intCast<int>(data.size()); }
int ScrollingBuffer::offset() const { return dataOffset; }

const float *ScrollingBuffer::xs() const { return &data[0].x; }
const float *ScrollingBuffer::ys() const { return &data[0].y; }
