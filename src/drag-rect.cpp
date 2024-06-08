#include "drag-rect.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "implot_internal.h"

#include <algorithm>
#include <tuple>

namespace plot {

constexpr float EdgeWidthPx = 20;
constexpr float HalfEdgeWidthPx = EdgeWidthPx / 2;

template <class T> static inline void set_pointer(T *ptr, T value)
{
    if (ptr != nullptr) {
        *ptr = value;
    }
}

// Coordinates in pixels
static bool drag_xrange(
    ImGuiID id,
    float& xmin,
    float& xmax,
    plot::DragXRectFlags flags,
    const ImRect& limits,
    bool& clicked,
    bool& hovered,
    bool& held
)
{
    const auto button_behaviour = [&](float x0, float x1) -> bool {
        ImGui::KeepAliveID(id);
        const ImRect bb(x0, limits.Min.y, x1, limits.Max.y);
        clicked = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        id += 1;
        return clicked || hovered || held;
    };

    if (ImHasFlag(flags, DragXRectFlag::NoInput)) {
        button_behaviour(xmin, xmax);
        return false;
    }

    const bool show_cursor = !ImHasFlag(flags, DragXRectFlag::NoCursor);
    const auto get_drag_delta = [](bool held) -> float {
        if (held && ImGui::IsMouseDragging(0)) {
            return ImGui::GetIO().MouseDelta.x;
        }
        return 0.;
    };

    // Movement
    if (button_behaviour(xmin + HalfEdgeWidthPx, xmax - HalfEdgeWidthPx)) {
        if ((held || hovered) && show_cursor) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        const float delta = get_drag_delta(held);
        if (delta) {
            xmin += delta;
            xmax += delta;
            if (xmin < limits.Min.x) {
                xmax = limits.Min.x + (xmax - xmin);
                xmin = limits.Min.x;
            } else if (xmax > limits.Max.x) {
                xmin = limits.Max.x - (xmax - xmin);
                xmax = limits.Max.x;
            }
            return true;
        }
        return false;
    }

    return false;
}

bool drag_xrange(
    int caller_id,
    double& xmin,
    double& xmax,
    const ImColor& color,
    DragXRectFlags flags,
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

    bool clicked = false;
    bool hovered = false;
    bool held = false;
    float xmin_px = current_plot->XAxis(0).PlotToPixels(xmin);
    float xmax_px = current_plot->XAxis(0).PlotToPixels(xmax);
    ImGuiID id = ImGui::GetCurrentWindow()->GetID(caller_id);

    const ImPlotRange& yrange = ImPlot::GetPlotLimits().Y;
    const ImPlotRange& xconstraint = current_plot->XAxis(0).ConstraintRange;
    const ImRect limits(
        ImPlot::PlotToPixels(xconstraint.Min, yrange.Max),
        ImPlot::PlotToPixels(xconstraint.Max, yrange.Min)
    );
    const bool modified = drag_xrange(id, xmin_px, xmax_px, flags, limits, clicked, hovered, held);
    ImPlot::GetPlotDrawList()->AddRectFilled(
        {xmin_px, limits.Min.y}, {xmax_px, limits.Max.y}, color
    );

    std::tie(xmin, xmax) = std::minmax(
        current_plot->XAxis(0).PixelsToPlot(xmin_px), current_plot->XAxis(0).PixelsToPlot(xmax_px)
    );

    set_pointer(out_clicked, clicked);
    set_pointer(out_hovered, hovered);
    set_pointer(out_held, held);
    ImGui::PopID();
    return modified;
}

bool drag_xrange(
    int id,
    ImPlotRange& xrange,
    const ImColor& color,
    plot::DragXRectFlags flags,
    bool *out_clicked,
    bool *out_hovered,
    bool *held
)
{
    return drag_xrange(id, xrange.Min, xrange.Max, color, flags, out_clicked, out_hovered, held);
}

}; // namespace plot
