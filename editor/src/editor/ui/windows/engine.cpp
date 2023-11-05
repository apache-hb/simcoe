#include "editor/ui/windows/engine.h"

#include "game/world.h"

#include "implot/implot.h"

using namespace editor;
using namespace editor::ui;

namespace {
    constexpr auto kTickNames = std::to_array<const char*>({
        "Render", "Physics", "Game"
    });
}

EngineDebug::EngineDebug(game::World *pWorld)
    : ServiceDebug("Engine")
    , pWorld(pWorld)
{
    ASSERTF(pWorld != nullptr, "World is null");

    renderStep  = 1.f / pWorld->renderStep.getDelta();
    physicsStep = 1.f / pWorld->physicsStep.getDelta();
    gameStep    = 1.f / pWorld->gameStep.getDelta();

    for (int i = 0; i < game::eTickCount; i++) {
        tickTimes[i].addPoint(0.f, 0.f);
    }
}

void EngineDebug::draw() {
    drawFrameTimes();
}

void EngineDebug::drawFrameTimes() {
    lastUpdate = float(ImGui::GetTime());

    game::TickAlert time;
    size_t limit = 20; // dont get more than 20 messages per tick
    while (pWorld->tickAlerts.try_dequeue(time) && limit-- > 0) {
        tickTimes[time.tick].addPoint(time.time, time.delta * 1000.f);

        // TODO: clock.now() seems to be giving us bunk values
        LOG_INFO("tick: {} time: {} delta: {}", kTickNames[time.tick], time.time, time.delta * 1000.f);
    }

    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0.f, 0.f));

    for (int tick = 0; tick < game::eTickCount; tick++) {
        ImGui::SeparatorText(kTickNames[tick]);
        if (ImPlot::BeginPlot(kTickNames[tick], ImVec2(0, 100), ImPlotFlags_CanvasOnly)) {
            ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoMenus);
            ImPlot::SetupAxisLimits(ImAxis_X1, lastUpdate - history, lastUpdate, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.f, 30.f, ImGuiCond_Always);
            ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
            ImPlot::PlotLine(kTickNames[tick], tickTimes[tick].xs(), tickTimes[tick].ys(), tickTimes[tick].size(), ImPlotLineFlags_Shaded, tickTimes[tick].offset(), ScrollingBuffer::getStride());
            ImPlot::PopStyleVar();
            ImPlot::EndPlot();
        }
    }

    ImPlot::PopStyleVar();
}
