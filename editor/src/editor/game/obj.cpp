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

std::vector<rhi::VertexAttribute> ObjMesh::getVertexAttributes() const {
    return {
        { "POSITION", offsetof(ObjVertex, position), rhi::TypeFormat::eFloat3 },
        { "TEXCOORD", offsetof(ObjVertex, uv), rhi::TypeFormat::eFloat2 }
    };
}

void ObjMesh::create() {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string error;

    bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &error, path.c_str(), basedir.c_str());

    if (!warn.empty()) {
        simcoe::logWarn("tinyobj warn {}", warn);
    }

    if (!error.empty()) {
        simcoe::logError("tinyobj error {}", error);
    }

    if (!ok) {
        simcoe::logError("failed to load obj {}", path);
        throw std::runtime_error("failed to load obj");
    }

    simcoe::logInfo("loaded obj {} (shapes={})", path, shapes.size());

    const auto& shape = shapes[0];
    std::vector<ObjVertex> vertexBuffer;
    std::vector<uint16_t> indexBuffer;

    const auto& vertices = attrib.vertices;
    const auto& indices = shape.mesh.indices;
    ASSERT(vertices.size() % 3 == 0);
    ASSERT(indices.size() % 3 == 0);

    simcoe::logInfo("(vertices={} indices={})", vertices.size(), indices.size());

    std::unordered_map<math::float3, uint16_t> vertexMap;

    std::mt19937 rand{std::random_device{}()};
    std::uniform_real_distribution<float> dist{0.0f, 1.0f};

    for (size_t v = 0; v < vertices.size(); v += 3) {
        auto v0 = vertices[v + 0];
        auto v1 = vertices[v + 1];
        auto v2 = vertices[v + 2];

        math::float3 vertex = { v0, v1, v2 };

        vertexBuffer.push_back({ vertex, { dist(rand), dist(rand) } });

        if (vertexMap.find(vertex) == vertexMap.end()) {
            vertexMap[vertex] = vertexBuffer.size() - 1;
        }

        simcoe::logInfo("vertex ({} {} {})", v0, v1, v2);
    }

    simcoe::logInfo("n-verts-per-face = {}", shape.mesh.num_face_vertices.size());
    size_t offset = 0;
    for (size_t p = 0; p < shape.mesh.num_face_vertices.size(); p++) {
        size_t verts = shape.mesh.num_face_vertices[p];
        simcoe::logInfo("n-verts-per-face[{}] = {}", p, verts);

        if (verts == 3) {
            auto i0 = indices[offset + 0];
            auto i1 = indices[offset + 1];
            auto i2 = indices[offset + 2];

            simcoe::logInfo("face ({} {} {})", i0.vertex_index, i1.vertex_index, i2.vertex_index);

            auto v0 = vertexBuffer[i0.vertex_index];
            auto v1 = vertexBuffer[i1.vertex_index];
            auto v2 = vertexBuffer[i2.vertex_index];

            indexBuffer.push_back(vertexMap[v0.position]);
            indexBuffer.push_back(vertexMap[v2.position]);
            indexBuffer.push_back(vertexMap[v1.position]);
        } else {
            auto i0 = indices[offset + 0];
            auto i1 = indices[offset + 1];
            auto i2 = indices[offset + 2];
            auto i3 = indices[offset + 3];

            simcoe::logInfo("face ({} {} {} {})", i0.vertex_index, i1.vertex_index, i2.vertex_index, i3.vertex_index);

            auto v0 = vertexBuffer[i0.vertex_index];
            auto v1 = vertexBuffer[i1.vertex_index];
            auto v2 = vertexBuffer[i2.vertex_index];
            auto v3 = vertexBuffer[i3.vertex_index];

            indexBuffer.push_back(vertexMap[v0.position]);
            indexBuffer.push_back(vertexMap[v1.position]);
            indexBuffer.push_back(vertexMap[v2.position]);

            indexBuffer.push_back(vertexMap[v0.position]);
            indexBuffer.push_back(vertexMap[v2.position]);
            indexBuffer.push_back(vertexMap[v3.position]);
        }

        offset += verts;
    }

    indexCount = indexBuffer.size();
    pVertexBuffer = ctx->createVertexBuffer(vertexBuffer.size(), sizeof(ObjVertex));
    pIndexBuffer = ctx->createIndexBuffer(indexBuffer.size(), rhi::TypeFormat::eUint16);

    std::unique_ptr<rhi::UploadBuffer> pVertexStaging{ctx->createUploadBuffer(vertexBuffer.data(), vertexBuffer.size() * sizeof(ObjVertex))};
    std::unique_ptr<rhi::UploadBuffer> pIndexStaging{ctx->createUploadBuffer(indexBuffer.data(), indexBuffer.size() * sizeof(uint16_t))};

    ctx->beginCopy();
    ctx->copyBuffer(pVertexBuffer, pVertexStaging.get());
    ctx->copyBuffer(pIndexBuffer, pIndexStaging.get());
    ctx->endCopy();
}

void ObjMesh::destroy() {
    delete pVertexBuffer;
    delete pIndexBuffer;
}

#if 0
    simcoe::logInfo("(vertices={} indices={})", vertices.size(), indices.size());

    std::unordered_map<math::float3, uint16_t> vertexMap;
    std::unordered_map<math::float2, uint16_t> uvMap;

    std::vector<ObjVertex> objVertexBuffer;
    std::vector<uint16_t> objIndexBuffer;

    std::unordered_map<ObjVertex, uint16_t> objMap;

    for (size_t v = 0; v < vertices.size(); v += 3) {
        auto v0 = vertices[v + 0];
        auto v1 = vertices[v + 1];
        auto v2 = vertices[v + 2];

        math::float3 vertex = { v0, v1, v2 };

        vertexBuffer.push_back({ vertex });

        if (vertexMap.find(vertex) == vertexMap.end()) {
            vertexMap[vertex] = vertexBuffer.size() - 1;
        }

        simcoe::logInfo("vertex ({} {} {})", v0, v1, v2);
    }

    for (size_t v = 0; v < uvs.size(); v += 2) {
        auto v0 = uvs[v + 0];
        auto v1 = uvs[v + 1];

        uvBuffer.push_back({ v0, v1 });

        if (uvMap.find({ v0, v1 }) == uvMap.end()) {
            uvMap[{ v0, v1 }] = uvBuffer.size() - 1;
        }

        simcoe::logInfo("uv ({} {})", v0, v1);
    }

    simcoe::logInfo("n-verts-per-face = {}", shape.mesh.num_face_vertices.size());
    size_t offset = 0;
    for (size_t p = 0; p < shape.mesh.num_face_vertices.size(); p++) {
        size_t verts = shape.mesh.num_face_vertices[p];
        simcoe::logInfo("n-verts-per-face[{}] = {}", p, verts);

        auto generateVertex = [&](tinyobj::index_t i) -> uint16_t {
            auto v0 = vertices[i.vertex_index + 3 + 0];
            auto v1 = vertices[i.vertex_index + 3 + 1];
            auto v2 = vertices[i.vertex_index + 3 + 2];

            auto uv0 = uvs[i.texcoord_index + 2 + 0];
            auto uv1 = uvs[i.texcoord_index + 2 + 1];

            math::float3 vertex = { v0, v1, v2 };
            math::float2 uv = { uv0, uv1 };

            ObjVertex objVertex = { vertex, uv };

            if (objMap.find(objVertex) == objMap.end()) {
                objMap[objVertex] = objVertexBuffer.size();
                objVertexBuffer.push_back(objVertex);
            }

            return objMap[objVertex];
        };

        if (verts == 3) {
            auto v0 = generateVertex(indices[offset + 0]);
            auto v1 = generateVertex(indices[offset + 1]);
            auto v2 = generateVertex(indices[offset + 2]);

            objIndexBuffer.push_back(v0);
            objIndexBuffer.push_back(v1);
            objIndexBuffer.push_back(v2);
        } else if (verts == 4) {
            auto v0 = generateVertex(indices[offset + 0]);
            auto v1 = generateVertex(indices[offset + 1]);
            auto v2 = generateVertex(indices[offset + 2]);
            auto v3 = generateVertex(indices[offset + 3]);

            objIndexBuffer.push_back(v0);
            objIndexBuffer.push_back(v1);
            objIndexBuffer.push_back(v2);

            objIndexBuffer.push_back(v0);
            objIndexBuffer.push_back(v2);
            objIndexBuffer.push_back(v3);
        } else {
            ASSERT(false);
        }

        offset += verts;
    }

    simcoe::logInfo("verts={} indices={}", objVertexBuffer.size(), objIndexBuffer.size());

    indexCount = objIndexBuffer.size();
    pVertexBuffer = ctx->createVertexBuffer(objVertexBuffer.size(), sizeof(ObjVertex));
    pIndexBuffer = ctx->createIndexBuffer(objIndexBuffer.size(), rhi::TypeFormat::eUint16);

#endif