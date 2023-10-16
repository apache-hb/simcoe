#pragma once

#include "engine/render/graph.h"

namespace simcoe::render {
    struct IMeshBufferHandle : IGraphObject {
        using IGraphObject::IGraphObject;
        virtual size_t getIndexCount() const = 0;

        virtual rhi::VertexBuffer *getVertexBuffer() const = 0;
        virtual rhi::IndexBuffer *getIndexBuffer() const = 0;
    };

    struct ISingleMeshBufferHandle : IMeshBufferHandle {
        using IMeshBufferHandle::IMeshBufferHandle;
        void destroy() override {
            delete pVertexBuffer;
            delete pIndexBuffer;
        }

        rhi::VertexBuffer *getVertexBuffer() const override {
            return pVertexBuffer;
        }

        rhi::IndexBuffer *getIndexBuffer() const override {
            return pIndexBuffer;
        }

    protected:
        void setVertexBuffer(rhi::VertexBuffer *pBuffer) {
            pVertexBuffer = pBuffer;
        }

        void setIndexBuffer(rhi::IndexBuffer *pBuffer) {
            pIndexBuffer = pBuffer;
        }
    private:
        rhi::VertexBuffer *pVertexBuffer;
        rhi::IndexBuffer *pIndexBuffer;
    };
}
