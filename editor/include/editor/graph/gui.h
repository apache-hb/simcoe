#pragma once

#include "editor/graph/assets.h"

namespace editor::graph {
    struct IGuiPass : IRenderPass {
        IGuiPass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pHandle);
        ~IGuiPass();

        virtual void content() = 0;

        void create() override;
        void destroy() override;

        void execute() override;

        static LRESULT handleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    private:
        ShaderResourceAlloc::Index guiUniformIndex;
        std::string iniPath;
    };
}
