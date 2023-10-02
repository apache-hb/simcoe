#include "editor/render/graph.h"

using namespace editor;

IRenderGraph::~IRenderGraph() {
    for (auto pPass : passes) {
        pPass->destroy(ctx);
        delete pPass;
    }
}

void IRenderGraph::addPass(IGraphPass *pPass) {
    pPass->create(ctx);
}

void IRenderGraph::execute(IGraphPass *pPass) {
    pPass->execute(ctx);
}
