#pragma once

#include "editor/graph/assets.h"

namespace editor::graph {
    struct IGuiPass : IRenderPass {
        IGuiPass(Graph *ctx, ResourceWrapper<IRTVHandle> *pHandle);
        ~IGuiPass();

        virtual void content() = 0;

        void create() override;
        void destroy() override;

        void execute() override;

        static LRESULT handleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    private:
        PassAttachment<IRTVHandle> *pHandle;
        ShaderResourceAlloc::Index guiUniformIndex;
    };
}
