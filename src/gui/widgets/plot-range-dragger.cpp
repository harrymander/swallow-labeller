#include "plot-range-dragger.hpp"

#include "util.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <implot_internal.h>

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>

namespace recap::labeller::gui::widgets {

namespace {

constexpr float EdgeWidthPx = 8;
constexpr float HalfEdgeWidthPx = EdgeWidthPx / 2;

template <class T> std::pair<T *, T *> minmax_pointers(T *v1, T *v2)
{
    return (*v1 <= *v2) ? std::make_pair(v1, v2) : std::make_pair(v2, v1);
}

// Coordinates in pixels
bool drag_range(ImGuiID id, float& x0, float& x1, const ImRect& limits, bool& held)
{
    bool hovered;

    const auto button_behaviour = [&](float x0, float x1, ImGuiMouseCursor cursor) -> bool {
        ImGui::KeepAliveID(id);
        const ImRect bb(x0, limits.Min.y, x1, limits.Max.y);
        (void) ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if (held || hovered) {
            ImGui::SetMouseCursor(cursor);
        }
        id += 1;
        return hovered || held;
    };

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
        if (delta != 0) {
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
    for (float *xp : {&x0, &x1}) {
        float& x = *xp;
        if (button_behaviour(x - HalfEdgeWidthPx, x + HalfEdgeWidthPx, ImGuiMouseCursor_ResizeEW)) {
            const float delta = get_drag_delta(held);
            if (delta != 0) {
                x = std::clamp(x + delta, limits.Min.x, limits.Max.x);
                return true;
            }
            return false;
        }
    }

    return false;
}

}; // namespace

PlotRangeDragger::PlotRangeDragger(PlotRange& range) : m_range(range) {}

bool PlotRangeDragger::update(ImGuiID id)
{
    const ImPlotPlot *current_plot = ImPlot::GetCurrentPlot();
    IM_ASSERT_USER_ERROR(
        current_plot != nullptr, "drag_xrect needs to be called between BeginPlot and EndPlot"
    );
    ImPlot::SetupLock();

    const ImPlotRect plot_limits = ImPlot::GetPlotLimits();
    const ImPlotRange& xconstraint = current_plot->XAxis(0).ConstraintRange;
    const ImPlotRange xlimit(
        std::isinf(xconstraint.Min) ? plot_limits.X.Min : xconstraint.Min,
        std::isinf(xconstraint.Max) ? plot_limits.X.Max : xconstraint.Max
    );

    double& xmin = m_range.start;
    double& xmax = m_range.end;
    float xmin_px = plot_xaxis_to_pixels(xmin);
    float xmax_px = plot_xaxis_to_pixels(xmax);
    bool held = false;
    const bool modified = drag_range(
        id,
        xmin_px,
        xmax_px,
        ImRect(
            ImPlot::PlotToPixels(xlimit.Min, plot_limits.Y.Max),
            ImPlot::PlotToPixels(xlimit.Max, plot_limits.Y.Min)
        ),
        held
    );

    if (modified) {
        xmin = current_plot->XAxis(0).PixelsToPlot(xmin_px);
        xmax = current_plot->XAxis(0).PixelsToPlot(xmax_px);
    }
    if (!held && xmin > xmax) {
        std::swap(xmin, xmax);
    }

    if (modified) {
        m_modified = true;
    }
    if (m_modified && !held) {
        m_modified = false;
        return true;
    }
    return false;
}

}; // namespace recap::labeller::gui::widgets
