#include "spdlog/spdlog.h"
#define IMGUI_DEFINE_MATH_OPERATORS

#include "implot-util.hpp"

#include <imgui.h>
#include <implot.h>
#include <implot_internal.h>

#include <algorithm>

namespace recap::implot_util {

void plot_vspan(double x0, double x1, const ImColor& color, float height_px)
{
    const ImVec2 plot0 = ImPlot::GetPlotPos();
    const ImVec2 plot1 = plot0 + ImPlot::GetPlotSize();

    const float y0 = height_px < 0 ? std::max(plot0.y, plot1.y + height_px) : plot0.y;
    const float y1 = height_px > 0 ? std::min(plot1.y, plot0.y + height_px) : plot1.y;
    const auto [xmin, xmax] = std::minmax(x0, x1);
    if (xmin != xmax && y0 != y1) {
        const ImPlotAxis& xaxis = ImPlot::GetCurrentPlot()->XAxis(0);
        ImPlot::PushPlotClipRect();
        ImPlot::GetPlotDrawList()->AddRectFilled(
            {std::clamp(xaxis.PlotToPixels(xmin), plot0.x, plot1.x), y0},
            {std::clamp(xaxis.PlotToPixels(xmax), plot0.x, plot1.x), y1},
            color
        );
        ImPlot::PopPlotClipRect();
    }
}

}; // namespace recap::implot_util
