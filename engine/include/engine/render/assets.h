#pragma once

#include "engine/render/graph.h"

namespace simcoe::render {
    template<typename T>
    struct IBufferHandle : IGraphObject {
        void destroy() override {
            delete pBuffer;
        }

        T *getBuffer() const { return pBuffer; }

    protected:
        void setBuffer(T *pBuffer) { this->pBuffer = pBuffer; }

    private:
        T *pBuffer = nullptr;
    };

    struct VertexBufferHandle final : IBufferHandle<rhi::VertexBuffer> {
        void create() override {

        }
    };

    struct IndexBufferHandle final : IBufferHandle<rhi::IndexBuffer> {
        void create() override {

        }
    };
}
