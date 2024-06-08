#include "selector.hpp"

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include <algorithm>
#include <cmath>

namespace plot {

static bool key_down_or_none(ImGuiKey key)
{
    return key == ImGuiKey_None || ImGui::IsKeyDown(key);
}

static ImPlotPlot& get_current_plot()
{
    ImPlotPlot *plot = ImPlot::GetCurrentPlot();
    IM_ASSERT_USER_ERROR(plot, "PlotXSelector::draw() needs to be called inside a plot");
    return *plot;
}

void PlotXSelector::draw(
    const ImColor& color, PlotSelectorFlags flags, ImGuiMouseButton button, ImGuiKey key
)
{
    auto plot = get_current_plot();
    IM_ASSERT_USER_ERROR(
        ImHasFlag(plot.Flags, ImPlotFlags_NoBoxSelect), "Box select must be disabled"
    );

    if (!(selecting || ImGui::IsItemHovered())) {
        return;
    }

    if (ImGui::IsMouseDown(button) && key_down_or_none(key)) {
        const double xmouse = ImPlot::GetPlotMousePos().x;
        if (selecting) {
            xmouse_drag = xmouse;
        } else {
            selecting = true;
            xmouse_drag = xmouse_start = xmouse;
        }
    } else if (selecting) {
        const auto [xmin, xmax] = std::minmax(xmouse_drag, xmouse_start);
        last_selection_.emplace(ImPlotRange(xmin, xmax));
        selecting = false;
    }

    if (selecting) {
        draw_selection(color);
        if (!ImHasFlag(flags, NoCursor)) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
    }
}

void PlotXSelector::draw_selection(const ImColor& color) const
{
    const auto yrange = ImPlot::GetPlotLimits().Y;
    const auto [xmin, xmax] = std::minmax(xmouse_drag, xmouse_start);
    ImPlot::GetPlotDrawList()->AddRectFilled(
        ImPlot::PlotToPixels(xmin, yrange.Min), ImPlot::PlotToPixels(xmax, yrange.Max), color
    );
}

bool PlotXSelector::is_selecting() const
{
    return selecting;
}

bool PlotXSelector::has_selected() const
{
    return last_selection_.has_value();
}

std::optional<ImPlotRange> PlotXSelector::last_selection() const
{
    return last_selection_;
}

void PlotXSelector::clear_selection()
{
    last_selection_.reset();
}

}; // namespace plot
