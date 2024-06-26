#include "selector.hpp"

#include "drag-range.hpp"
#include "imgui-util.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <implot_internal.h>

#include <algorithm>
#include <cmath>
#include <tuple>

namespace recap::plot {

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
    ImGuiID caller_id,
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

    ImPlot::SetupLock();
    const ImPlotAxis& x_axis = current_plot.XAxis(0);
    const ImPlotRect plot_limits = ImPlot::GetPlotLimits();

    imgui_util::ScopedImID scoped_id("#PLOT_DRAG_XSELECTOR");
    ImGuiID id = ImGui::GetCurrentWindow()->GetID(caller_id);
    const auto set_active = [id]() {
        ImGui::KeepAliveID(id);
        ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
    };

    const bool last_selecting = selecting;
    bool cancelled = false;
    const bool key_down = key_down_or_none(key);
    const float mouse_pos = ImGui::GetMousePos().x;
    float xmin_px = NAN;
    float xmax_px = NAN;
    if (selecting) {
        if (ImHasFlag(flags, CancelOnKeyRelease) && !key_down) {
            cancelled = true;
            selecting = false;
        } else if (ImGui::IsMouseDragging(mouse_button)) {
            set_active();
            const float clicked_pos = mouse_pos - ImGui::GetMouseDragDelta(mouse_button).x;
            const float position_clamped = std::clamp(
                mouse_pos,
                x_axis.PlotToPixels(plot_limits.X.Min),
                x_axis.PlotToPixels(plot_limits.X.Max)
            );
            std::tie(xmin_px, xmax_px) = std::minmax(clicked_pos, position_clamped);
            ImGui::ClearActiveID();
        } else if (!ImGui::IsMouseDown(mouse_button)) {
            selecting = false;
        }
    } else if (key_down && ImPlot::IsPlotHovered() && ImGui::IsMouseDown(mouse_button)) {
        set_active();
        selecting = true;
        xmin_px = xmax_px = mouse_pos;
        ImGui::ClearActiveID();
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

    return !cancelled && last_selecting && !selecting;
}

bool PlotXSelector::is_selecting() const
{
    return selecting;
}

}; // namespace recap::plot
