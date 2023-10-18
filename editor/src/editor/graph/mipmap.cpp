#include "editor/graph/mipmap.h"

using namespace editor;
using namespace editor::graph;

MipMapInfoHandle::MipMapInfoHandle(Graph *graph)
    : IUniformHandle(graph, "mipmap.info")
{ }

MipMapPass::MipMapPass(Graph *ctx, ResourceWrapper<ITextureHandle> *pSourceTexture)
    : ICommandPass(ctx, "mipmap")
    , pSourceTexture(addAttachment(pSourceTexture, rhi::ResourceState::eTexture))
{
    pMipMapInfo = graph->addResource<MipMapInfoHandle>();
    pMipMapInfoAttachment = addAttachment(pMipMapInfo, rhi::ResourceState::eUniform);
}

void MipMapPass::create() {
    const auto& info = ctx->getCreateInfo();

    const rhi::ComputePipelineInfo createInfo = {
        .computeShader = info.depot.loadBlob("mipmap.cs.cso"),

        .textureInputs = {
            { "src", rhi::InputVisibility::eCompute, 0, true }
        },
        .uniformInputs = {
            { "info", rhi::InputVisibility::eCompute, 0, true }
        },
        .uavInputs = {
            { "dst", rhi::InputVisibility::eCompute, 0, false }
        },
        .samplers = {
            { rhi::InputVisibility::eCompute, 0 }
        }
    };

    pPipelineState = ctx->createComputePipeline(createInfo);
}

void MipMapPass::destroy() {
    delete pPipelineState;
}

void MipMapPass::execute() {
    ctx->setComputePipeline(pPipelineState);
}
