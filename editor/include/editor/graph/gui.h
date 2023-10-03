#pragma once

#include "editor/graph/assets.h"

namespace editor::graph {
    struct IGuiPass : IRenderPass {
        IGuiPass(IResourceHandle *pHandle);
        ~IGuiPass();

        virtual void content() = 0;

        void create(RenderContext *ctx) override;
        void destroy(RenderContext *ctx) override;

        void execute(RenderContext *ctx) override;

        static LRESULT handleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    private:
        PassResource<IResourceHandle> *pHandle;
        DataAlloc::Index guiUniformIndex;
    };
}
