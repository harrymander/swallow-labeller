#include "drag-rect.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "implot_internal.h"

#include <algorithm>
#include <tuple>

namespace plot {

constexpr float DragWidthPx = 20;

static float
rect_drag_delta(const ImRect& rect, ImGuiID id, bool& clicked, bool& hovered, bool& held)
{
    ImGui::KeepAliveID(id);
    clicked = ImGui::ButtonBehavior(rect, id, &hovered, &held);
    if (held && ImGui::IsMouseDragging(0)) {
        return ImGui::GetIO().MouseDelta.x;
    }
    return 0.;
}

static bool
move_edge(ImGuiID id, float& x, float ymin, float ymax, bool& clicked, bool& hovered, bool& held)
{
    const ImRect rect(x - DragWidthPx / 2, ymin, x + DragWidthPx / 2, ymax);
    const float delta = rect_drag_delta(rect, id, clicked, hovered, held);
    if (delta) {
        x += delta;
        return true;
    }
    return false;
}

static bool
resize(ImGuiID& id, ImVec2& px_min, ImVec2& px_max, bool& clicked, bool& hovered, bool& held)
{
    bool modified = move_edge(id, px_min.x, px_min.y, px_max.y, clicked, hovered, held);
    id += 1;
    if (move_edge(id, px_max.x, px_min.y, px_max.y, clicked, hovered, held)) {
        modified = true;
    }
    return modified;
}

// Returns delta in pixels
static float move_delta(
    ImGuiID& id,
    const ImVec2& px_min,
    const ImVec2& px_max,
    bool& clicked,
    bool& hovered,
    bool& held
)
{
    const ImRect rect(px_min.x + DragWidthPx / 2, px_min.y, px_max.x - DragWidthPx / 2, px_max.y);
    const float delta = rect_drag_delta(rect, id, clicked, hovered, held);
    id += 1;
    return delta;
}

bool drag_xrange(
    int caller_id,
    double& xmin,
    double& xmax,
    const ImColor& color,
    plot::DragXRectFlags flags,
    bool *out_clicked,
    bool *out_hovered,
    bool *out_held
)
{
    const ImPlotPlot *current_plot = ImPlot::GetCurrentPlot();
    IM_ASSERT_USER_ERROR(
        current_plot != nullptr, "drag_xrect needs to be called between BeginPlot and EndPlot"
    );
    ImGui::PushID("#PLOT_DRAG_XRANGE");
    ImPlot::SetupLock();

    const auto plot_limits = ImPlot::GetPlotLimits();
    ImVec2 px_min = ImPlot::PlotToPixels(xmin, plot_limits.Y.Max);
    ImVec2 px_max = ImPlot::PlotToPixels(xmax, plot_limits.Y.Min);

    const bool cursor = !ImHasFlag(flags, DragXRectFlag::NoCursor);
    bool clicked = false;
    bool hovered = false;
    bool held = false;
    bool modified = false;

    ImGuiID id = ImGui::GetCurrentWindow()->GetID(caller_id);
    if (!ImHasFlag(flags, DragXRectFlag::NoMove)) {
        const float xdelta = move_delta(id, px_min, px_max, clicked, hovered, held);
        if ((hovered || held) && cursor) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        if (xdelta) {
            px_min.x += xdelta;
            px_max.x += xdelta;
            modified = true;
        }
    }

    if (!ImHasFlag(flags, DragXRectFlag::NoResize)) {
        modified = resize(id, px_min, px_max, clicked, hovered, held);
        if ((hovered || held) && cursor) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
    }

    std::tie(xmin, xmax) =
        std::minmax(ImPlot::PixelsToPlot(px_min).x, ImPlot::PixelsToPlot(px_max).x);
    const auto& xconstraint = current_plot->XAxis(0).ConstraintRange;
    if (xmin < xconstraint.Min) {
        xmax = xmax + xconstraint.Min - xmin;
        xmin = xconstraint.Min;
    } else if (xmax > xconstraint.Max) {
        xmin = xmin - xmax + xconstraint.Max;
        xmax = xconstraint.Max;
    }
    ImPlot::GetPlotDrawList()->AddRectFilled(
        ImPlot::PlotToPixels(std::max(xmin, plot_limits.X.Min), plot_limits.Y.Max),
        ImPlot::PlotToPixels(std::min(xmax, plot_limits.X.Max), plot_limits.Y.Min),
        color
    );

    if (out_clicked) {
        *out_clicked = clicked;
    }
    if (out_hovered) {
        *out_hovered = hovered;
    }
    if (out_held) {
        *out_held = held;
    }

    ImGui::PopID();
    return modified;
}

bool drag_xrange(
    int id,
    ImPlotRange& xrange,
    const ImColor& color,
    plot::DragXRectFlags flag,
    bool *out_clicked,
    bool *out_hovered,
    bool *held
)
{
    return drag_xrange(id, xrange.Min, xrange.Max, color, flag, out_clicked, out_hovered, held);
}

}; // namespace plot
