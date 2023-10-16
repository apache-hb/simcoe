#include "editor/game/object.h"

#include "tinyobj/loader.h"
#include <unordered_map>

#include <random>

using namespace editor;

template<>
struct std::hash<math::float3> {
    size_t operator()(const math::float3& v) const {
        return std::hash<float>()(v.x) ^ std::hash<float>()(v.y) ^ std::hash<float>()(v.z);
    }
};

template<>
struct std::hash<math::float2> {
    size_t operator()(const math::float2& v) const {
        return std::hash<float>()(v.x) ^ std::hash<float>()(v.y);
    }
};

template<>
struct std::hash<ObjVertex> {
    size_t operator()(const ObjVertex& v) const {
        return std::hash<math::float3>()(v.position) ^ std::hash<math::float2>()(v.uv);
    }
};

template<>
struct std::equal_to<ObjVertex> {
    bool operator()(const ObjVertex& lhs, const ObjVertex& rhs) const {
        return lhs.position == rhs.position && lhs.uv == rhs.uv;
    }
};

void ObjMesh::create() {
    const auto& createInfo = ctx->getCreateInfo();
    auto assetPath = createInfo.depot.getAssetPath(path);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string error;

    bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &error, assetPath.string().c_str(), assetPath.parent_path().string().c_str());

    if (!warn.empty()) {
        simcoe::logWarn("tinyobj warn {}", warn);
    }

    if (!error.empty()) {
        simcoe::logError("tinyobj error {}", error);
    }

    if (!ok) {
        simcoe::logError("failed to load obj {}", path.string());
        throw std::runtime_error("failed to load obj");
    }

    simcoe::logInfo("loaded obj {} (shapes={})", path.string(), shapes.size());
    ASSERT(shapes.size() >= 1);

    const auto& shape = shapes[0];
    std::vector<ObjVertex> vertexBuffer;
    std::vector<uint16_t> indexBuffer;

    std::unordered_map<ObjVertex, uint16_t> uniqueVertices;

    const auto& vertices = attrib.vertices;
    const auto& texcoords = attrib.texcoords;
    const auto& indices = shape.mesh.indices;
    ASSERT(vertices.size() % 3 == 0);
    ASSERT(indices.size() % 3 == 0);

    simcoe::logInfo("(vertices={} uvs={} indices={})", vertices.size(), texcoords.size(), indices.size());

    auto getUvCoord = [&](int idx) {
        if (idx != -1) {
            return math::float2{ texcoords[idx * 2 + 0], texcoords[idx * 2 + 1] };
        } else {
            return math::float2{ 0.0f, 0.0f };
        }
    };

    auto getIndex = [&](tinyobj::index_t idx) {
        auto vtx = idx.vertex_index;
        auto uv = idx.texcoord_index;

        math::float2 uvCoord = getUvCoord(uv);
        math::float3 position = { vertices[vtx * 3 + 0], vertices[vtx * 3 + 1], vertices[vtx * 3 + 2] };

        ObjVertex vertex = { position, uvCoord };
        if (uniqueVertices.find(vertex) == uniqueVertices.end()) {
            uniqueVertices[vertex] = static_cast<uint16_t>(vertexBuffer.size());
            vertexBuffer.push_back(vertex);
        }

        return uniqueVertices[vertex];
    };

    size_t offset = 0;
    for (size_t i = 0; i < shape.mesh.num_face_vertices.size(); i++) {
        size_t verts = shape.mesh.num_face_vertices[i];
        if (verts == 3) {
            auto i0 = getIndex(indices[offset + 0]);
            auto i1 = getIndex(indices[offset + 1]);
            auto i2 = getIndex(indices[offset + 2]);

            indexBuffer.push_back(i0);
            indexBuffer.push_back(i1);
            indexBuffer.push_back(i2);
        } else if (verts == 4) {
            auto i0 = getIndex(indices[offset + 0]);
            auto i1 = getIndex(indices[offset + 1]);
            auto i2 = getIndex(indices[offset + 2]);
            auto i3 = getIndex(indices[offset + 3]);

            indexBuffer.push_back(i0);
            indexBuffer.push_back(i1);
            indexBuffer.push_back(i2);

            indexBuffer.push_back(i0);
            indexBuffer.push_back(i2);
            indexBuffer.push_back(i3);
        } else {
            ASSERTF(false, "unsupported face vertex count {}", verts);
        }

        offset += verts;
    }

    simcoe::logInfo("buffer sizes (vertices={} indices={})", vertexBuffer.size(), indexBuffer.size());

    indexCount = indexBuffer.size();
    pVertexBuffer = ctx->createVertexBuffer(vertexBuffer.size(), sizeof(ObjVertex));
    pIndexBuffer = ctx->createIndexBuffer(indexBuffer.size(), rhi::TypeFormat::eUint16);

    std::unique_ptr<rhi::UploadBuffer> pVertexStaging{ctx->createUploadBuffer(vertexBuffer.data(), vertexBuffer.size() * sizeof(ObjVertex))};
    std::unique_ptr<rhi::UploadBuffer> pIndexStaging{ctx->createUploadBuffer(indexBuffer.data(), indexBuffer.size() * sizeof(uint16_t))};

    pIndexBuffer->setName(std::format("ibo({})", path.string()));
    pVertexBuffer->setName(std::format("vbo({})", path.string()));

    pIndexStaging->setName(std::format("ibo-staging({})", path.string()));
    pVertexStaging->setName(std::format("vbo-staging({})", path.string()));

    ctx->beginCopy();
    ctx->copyBuffer(pVertexBuffer, pVertexStaging.get());
    ctx->copyBuffer(pIndexBuffer, pIndexStaging.get());
    ctx->endCopy();
}

void ObjMesh::destroy() {
    delete pVertexBuffer;
    delete pIndexBuffer;
}
