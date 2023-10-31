#include "editor/debug/service.h"

#include "game/world.h"

#include "implot/implot.h"

using namespace editor;
using namespace editor::debug;

EngineDebug::EngineDebug(game::World *pWorld)
    : ServiceDebug("Engine")
    , pWorld(pWorld)
{
    ASSERTF(pWorld != nullptr, "World is null");

    inputStep   = 1.f / pWorld->inputStep.getDelta();
    renderStep  = 1.f / pWorld->renderStep.getDelta();
    physicsStep = 1.f / pWorld->physicsStep.getDelta();
    gameStep    = 1.f / pWorld->gameStep.getDelta();
}

void EngineDebug::draw() {
    ImGui::PushItemWidth(200.f);

    if (ImGui::SliderFloat("Input tps", &inputStep, 1.f, 400.f)) {
        pWorld->pInputThread->add("set-input-step", [this, tps = 1.f / inputStep] {
            pWorld->inputStep.updateDelta(tps);
        });
    }

    if (ImGui::SliderFloat("Physics tps", &physicsStep, 1.f, 400.f)) {
        pWorld->pPhysicsThread->add("set-physics-step", [this, tps = 1.f / physicsStep] {
            pWorld->physicsStep.updateDelta(tps);
        });
    }

    if (ImGui::SliderFloat("Game tps", &gameStep, 1.f, 400.f)) {
        pWorld->pGameThread->add("set-game-step", [this, tps = 1.f / gameStep] {
            pWorld->gameStep.updateDelta(tps);
        });
    }

    ImGui::PopItemWidth();

    drawFrameTimes();
}

void EngineDebug::drawFrameTimes() {
    ImGui::SeparatorText("Frame times");

    float now = ImGui::GetTime();
    float delta = now - lastUpdate;
    lastUpdate = now;

    ImGui::Text("Frame time: %.3f ms", delta * 1000.f);

    frameTimes.AddPoint(lastUpdate, delta * 1000.f);

    const char *id = "Frame times";

    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0.f, 0.f));
    if (ImPlot::BeginPlot(id, ImVec2(500, 200), ImPlotFlags_CanvasOnly)) {
        ImPlot::SetupAxes(nullptr,"Frametime (ms)", ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoMenus);
        ImPlot::SetupAxisLimits(ImAxis_X1, lastUpdate - history, lastUpdate, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.f, 100.f, ImGuiCond_Always);
        ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
        ImPlot::PlotLine(id, &frameTimes.Data[0].x, &frameTimes.Data[0].y, frameTimes.Data.size(), ImPlotLineFlags_Shaded, frameTimes.Offset, 2 * sizeof(float));
        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleVar();

    ImGui::PushItemWidth(200.f);

    if (ImGui::SliderFloat("Render tps", &renderStep, 1.f, 400.f)) {
        pWorld->pRenderThread->add("set-render-step", [this, tps = 1.f / renderStep] {
            pWorld->renderStep.updateDelta(tps);
        });
    }

    ImGui::PopItemWidth();
}
