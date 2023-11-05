#pragma once

#include <vector>
#include "imgui/imgui.h"

namespace editor::ui {
    struct ScrollingBuffer {
        ScrollingBuffer(int size = 2000);

        void addPoint(float x, float y);
        void reset();

        int size() const;
        int offset() const;

        const float *xs() const;
        const float *ys() const;

        static int getStride() { return sizeof(ImVec2); }

    private:
        int maxSize;
        int dataOffset;
        std::vector<ImVec2> data;
    };
}
