#include "drag-rect.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "implot_internal.h"

#include <algorithm>
#include <tuple>

namespace plot {

constexpr float EdgeWidthPx = 10;
constexpr float HalfEdgeWidthPx = EdgeWidthPx / 2;

template <class T> static inline void set_pointer(T *ptr, T value)
{
    if (ptr != nullptr) {
        *ptr = value;
    }
}

template <class Comparable>
static inline std::pair<Comparable *, Comparable *> minmax_pointers(Comparable *v1, Comparable *v2)
{
    return (*v1 < *v2) ? std::make_pair(v1, v2) : std::make_pair(v2, v1);
}

// Coordinates in pixels
static bool drag_xrange(
    ImGuiID id,
    float& x0,
    float& x1,
    plot::DragXRectFlags flags,
    const ImRect& limits,
    bool& clicked,
    bool& hovered,
    bool& held
)
{
    const bool show_cursor = !ImHasFlag(flags, DragXRectFlag::NoCursor);
    const auto button_behaviour = [&](float x0, float x1, ImGuiMouseCursor cursor) -> bool {
        ImGui::KeepAliveID(id);
        const ImRect bb(x0, limits.Min.y, x1, limits.Max.y);
        clicked = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if ((held || hovered) && show_cursor && cursor != ImGuiMouseCursor_None) {
            ImGui::SetMouseCursor(cursor);
        }
        id += 1;
        return clicked || hovered || held;
    };

    // No input: just catch button activity on region
    if (ImHasFlag(flags, DragXRectFlag::NoInput)) {
        button_behaviour(x0, x1, ImGuiMouseCursor_None);
        return false;
    }

    const auto get_drag_delta = [](bool held) -> float {
        if (held && ImGui::IsMouseDragging(0)) {
            return ImGui::GetIO().MouseDelta.x;
        }
        return 0.;
    };

    // Movement
    float *xmin;
    float *xmax;
    std::tie(xmin, xmax) = minmax_pointers(&x0, &x1);
    if (button_behaviour(*xmin + HalfEdgeWidthPx, *xmax - HalfEdgeWidthPx, ImGuiMouseCursor_Hand)) {
        const float delta = get_drag_delta(held);
        if (delta) {
            *xmin += delta;
            *xmax += delta;
            if (*xmin < limits.Min.x) {
                *xmax = limits.Min.x + (*xmax - *xmin);
                *xmin = limits.Min.x;
            } else if (*xmax > limits.Max.x) {
                *xmin = limits.Max.x - (*xmax - *xmin);
                *xmax = limits.Max.x;
            }
            return true;
        }
        return false;
    }

    // Resizing
    float *edges[] = {&x0, &x1};
    for (int i = 0; i < 2; i++) {
        float& x = *edges[i];
        if (button_behaviour(x - HalfEdgeWidthPx, x + HalfEdgeWidthPx, ImGuiMouseCursor_ResizeEW)) {
            const float delta = get_drag_delta(held);
            if (delta) {
                x = std::clamp(x + delta, limits.Min.x, limits.Max.x);
                return true;
            }
            return false;
        }
    }

    return false;
}

static ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
{
    return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}

static void draw_plot_vspan(float x0, float x1, const ImColor& color)
{
    const ImVec2 top_left = ImPlot::GetPlotPos();
    const ImVec2 bottom_right = top_left + ImPlot::GetPlotSize();
    const auto [xmin, xmax] = std::minmax(x0, x1);
    const auto [lo, hi] = std::minmax(top_left.x, bottom_right.x);
    if (x0 != x1) {
        ImPlot::GetPlotDrawList()->AddRectFilled(
            {std::clamp(xmin, lo, hi), top_left.y},
            {std::clamp(xmax, lo, hi), bottom_right.y},
            color
        );
    }
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

    const ImPlotRect plot_limits = ImPlot::GetPlotLimits();
    const ImPlotRange& xconstraint = current_plot->XAxis(0).ConstraintRange;
    const bool modified = drag_xrange(
        id,
        xmin_px,
        xmax_px,
        flags,
        ImRect(
            ImPlot::PlotToPixels(xconstraint.Min, plot_limits.Y.Max),
            ImPlot::PlotToPixels(xconstraint.Max, plot_limits.Y.Min)
        ),
        clicked,
        hovered,
        held
    );

    draw_plot_vspan(xmin_px, xmax_px, color);
    xmin = current_plot->XAxis(0).PixelsToPlot(xmin_px);
    xmax = current_plot->XAxis(0).PixelsToPlot(xmax_px);
    if (!held && xmin > xmax) {
        std::swap(xmin, xmax);
    }

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
