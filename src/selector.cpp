#include "selector.hpp"

#include "drag-range.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "implot_internal.h"

#include <algorithm>
#include <cmath>
#include <tuple>

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

bool PlotXSelector::draw(
    int caller_id,
    ImPlotRange& range,
    const ImColor& color,
    PlotSelectorFlags flags,
    ImGuiMouseButton mouse_button,
    ImGuiKey key
)
{
    const ImPlotPlot& current_plot = get_current_plot();
    IM_ASSERT_USER_ERROR(
        ImHasFlag(current_plot.Flags, ImPlotFlags_NoBoxSelect), "Box select must be disabled"
    );

    ImGui::PushID("#PLOT_DRAG_XSELECTOR");
    ImPlot::SetupLock();
    const ImPlotAxis& x_axis = current_plot.XAxis(0);
    const ImPlotRect plot_limits = ImPlot::GetPlotLimits();
    const bool last_selecting = selecting;
    float xmin_px = NAN;
    float xmax_px = NAN;
    if (key_down_or_none(key)) {
        ImGuiID id = ImGui::GetCurrentWindow()->GetID(caller_id);
        const auto set_active = [id]() {
            ImGui::KeepAliveID(id);
            ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
        };

        const float position = ImGui::GetMousePos().x;
        if (selecting) {
            if (ImGui::IsMouseDragging(mouse_button)) {
                set_active();
                const float clicked_pos = position - ImGui::GetMouseDragDelta(mouse_button).x;
                const float position_clamped = std::clamp(
                    position,
                    x_axis.PlotToPixels(plot_limits.X.Min),
                    x_axis.PlotToPixels(plot_limits.X.Max)
                );
                std::tie(xmin_px, xmax_px) = std::minmax(clicked_pos, position_clamped);
            } else if (!ImGui::IsMouseDown(mouse_button)) {
                selecting = false;
            }
        } else if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(mouse_button)) {
            set_active();
            selecting = true;
            xmin_px = xmax_px = position;
        }
    } else {
        cancelled = ImHasFlag(flags, CancelOnKeyRelease);
        selecting = false;
    }

    if (selecting) {
        if (!(std::isnan(xmin_px) || std::isnan(xmax_px))) {
            range.Min = x_axis.PixelsToPlot(xmin_px);
            range.Max = x_axis.PixelsToPlot(xmax_px);
            ImPlot::PushPlotClipRect();
            ImPlot::GetPlotDrawList()->AddRectFilled(
                ImPlot::PlotToPixels(range.Min, plot_limits.Y.Min),
                ImPlot::PlotToPixels(range.Max, plot_limits.Y.Max),
                color
            );
            ImPlot::PopPlotClipRect();
        }

        if (!ImHasFlag(flags, NoCursor)) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
    }

    ImGui::PopID();
    const bool retval = !cancelled && last_selecting && !selecting;
    cancelled = false;
    return retval;
}

bool PlotXSelector::is_selecting() const
{
    return selecting;
}

void PlotXSelector::cancel()
{
    cancelled = true;
    selecting = false;
}

}; // namespace plot
