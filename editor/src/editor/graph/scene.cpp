#include "editor/graph/scene.h"

#include <array>

using namespace editor;
using namespace editor::graph;

///
/// create display helper functions
///

constexpr render::Display createDisplay(UINT width, UINT height) {
    render::Viewport viewport = {
        .width = float(width),
        .height = float(height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    render::Scissor scissor = {
        .right = LONG(width),
        .bottom = LONG(height)
    };

    return { viewport, scissor };
}

static constexpr math::float4 kClearColour = { 0.0f, 0.2f, 0.4f, 1.0f };

static constexpr auto kQuadVerts = std::to_array<Vertex>({
    Vertex{ { 0.5f, -0.5f, 0.0f }, { 0.f, 0.f } }, // top left
    Vertex{ { 0.5f, 0.5f, 0.0f }, { 1.f, 0.f } }, // top right
    Vertex{ { -0.5f, -0.5f, 0.0f }, { 0.f, 1.f } }, // bottom left
    Vertex{ { -0.5f, 0.5f, 0.0f }, { 1.f, 1.f } } // bottom right
});

static constexpr auto kQuadIndices = std::to_array<uint16_t>({
    0, 2, 1,
    1, 2, 3
});

struct UNIFORM_ALIGN UniformData {
    math::float2 offset;

    float angle;
    float aspect;
};

ScenePass::ScenePass(graph::SceneTargetHandle *pSceneTarget, graph::TextureHandle *pTexture) 
    : IRenderPass()
    , pSceneTarget(addResource<graph::SceneTargetHandle>(pSceneTarget, render::ResourceState::eRenderTarget))
    , pTextureHandle(addResource<graph::TextureHandle>(pTexture, render::ResourceState::eShaderResource))
{ }

void ScenePass::create(RenderContext *ctx) {
    const auto& createInfo = ctx->getCreateInfo();
    pTimer = new simcoe::Timer();
    display = createDisplay(createInfo.renderWidth, createInfo.renderHeight);

    // create pipeline
    const render::PipelineCreateInfo psoCreateInfo = {
        .vertexShader = createInfo.depot.loadBlob("quad.vs.cso"),
        .pixelShader = createInfo.depot.loadBlob("quad.ps.cso"),

        .attributes = {
            { "POSITION", offsetof(Vertex, position), render::TypeFormat::eFloat3 },
            { "TEXCOORD", offsetof(Vertex, uv), render::TypeFormat::eFloat2 }
        },

        .textureInputs = {
            { render::InputVisibility::ePixel, 0, true }
        },

        .uniformInputs = {
            { render::InputVisibility::eVertex, 0, false }
        },

        .samplers = {
            { render::InputVisibility::ePixel, 0 }
        }
    };

    pPipeline = ctx->createPipelineState(psoCreateInfo);

    // create uniform buffer

    pQuadUniformBuffer = ctx->createUniformBuffer(sizeof(UniformData));
    quadUniformIndex = ctx->mapUniform(pQuadUniformBuffer, sizeof(UniformData));

    // create vertex data
    std::unique_ptr<render::UploadBuffer> pVertexStaging{ctx->createUploadBuffer(kQuadVerts.data(), kQuadVerts.size() * sizeof(Vertex))};
    std::unique_ptr<render::UploadBuffer> pIndexStaging{ctx->createUploadBuffer(kQuadIndices.data(), kQuadIndices.size() * sizeof(uint16_t))};

    pQuadVertexBuffer = ctx->createVertexBuffer(kQuadVerts.size(), sizeof(Vertex));
    pQuadIndexBuffer = ctx->createIndexBuffer(kQuadIndices.size(), render::TypeFormat::eUint16);

    ctx->beginCopy();
    ctx->copyBuffer(pQuadVertexBuffer, pVertexStaging.get());
    ctx->copyBuffer(pQuadIndexBuffer, pIndexStaging.get());
    ctx->endCopy();
}

void ScenePass::destroy(RenderContext *ctx) {
    delete pPipeline;
    delete pQuadUniformBuffer;

    delete pQuadVertexBuffer;
    delete pQuadIndexBuffer;

    delete pTimer;
}

void ScenePass::execute(RenderContext *ctx) {
    updateUniform(ctx);

    IResourceHandle *pTarget = pSceneTarget->getHandle();
    IResourceHandle *pTexture = pTextureHandle->getHandle();

    ctx->setPipeline(pPipeline);
    ctx->setDisplay(display);
    ctx->setRenderTarget(pTarget->getRtvIndex(ctx), kClearColour);

    ctx->setShaderInput(pTexture->srvIndex, 0);
    ctx->setShaderInput(quadUniformIndex, 1);

    ctx->setVertexBuffer(pQuadVertexBuffer);
    ctx->drawIndexBuffer(pQuadIndexBuffer, kQuadIndices.size());
}

void ScenePass::updateUniform(RenderContext *ctx) {
    float time = pTimer->now();
    const auto& createInfo = ctx->getCreateInfo();

    UniformData data = {
        .offset = { 0.f, std::sin(time) / 3 },
        .angle = time,
        .aspect = float(createInfo.renderHeight) / float(createInfo.renderWidth)
    };

    pQuadUniformBuffer->write(&data, sizeof(UniformData));
}
