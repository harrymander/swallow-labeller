#include "drag-rect.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "implot_internal.h"

namespace plot {

constexpr float DragPxWidth = 20;

static double
rect_drag_delta(const ImRect& rect, ImGuiID id, bool& clicked, bool& hovered, bool& held)
{
    ImGui::KeepAliveID(id);
    clicked = ImGui::ButtonBehavior(rect, id, &hovered, &held);
    if (held && ImGui::IsMouseDragging(0)) {
        const auto mouse_delta = ImGui::GetIO().MouseDelta;
        return ImPlot::PixelsToPlot(mouse_delta).x;
    }
    return 0.;
}

// static bool resize_xrange_edge(
//     ImGuiID id,
//     double *x,
//     float x_px,
//     float y_px_min,
//     float y_px_max,
//     bool *clicked,
//     bool *hovered,
//     bool *held
// )
// {
//     const ImRect rect(x_px - DragPxWidth / 2, y_px_min, x_px + DragPxWidth / 2, y_px_max);
//     const double delta = rect_drag_delta(rect, id, clicked, hovered, held);
//     if (delta) {
//         *x += delta;
//         return true;
//     }
//     return false;
// }

// static bool resize_xrange(
//     ImGuiID id,
//     double *xmin,
//     double *xmax,
//     const ImVec2& px_min,
//     const ImVec2& px_max,
//     bool *clicked,
//     bool *hovered,
//     bool *held
// )
// {
//     bool modified =
//         resize_xrange_edge(id + 1, xmin, px_min.x, px_min.y, px_max.y, clicked, hovered, held);
//     if (resize_xrange_edge(id + 2, xmax, px_max.x, px_min.y, px_max.y, clicked, hovered, held))
//         modified = true;

//     if (*xmin > *xmax) {
//         std::swap(*xmin, *xmax);
//     }

//     return modified;
// }

// Returns delta in plot coordinates x-axis
static double move_xrange(
    ImGuiID id, const ImVec2& px_min, const ImVec2& px_max, bool& clicked, bool& hovered, bool& held
)
{
    const ImRect rect(px_min, px_max);
    return rect_drag_delta(rect, id, clicked, hovered, held);
}

static void draw_plot_vspan(double xmin, double xmax, const ImColor& color)
{
    const auto yrange = ImPlot::GetPlotLimits().Y;
    ImPlot::GetPlotDrawList()->AddRectFilled(
        ImPlot::PlotToPixels(xmin, yrange.Min), ImPlot::PlotToPixels(xmax, yrange.Max), color
    );
}

bool drag_xrange(
    int caller_id,
    double *xmin,
    double *xmax,
    const ImColor& color,
    plot::DragXRectFlags flags,
    bool *out_clicked,
    bool *out_hovered,
    bool *out_held
)
{
    IM_ASSERT_USER_ERROR(
        ImPlot::GetCurrentPlot() != nullptr,
        "drag_xrect needs to be called between BeginPlot and EndPlot"
    );
    ImGui::PushID("#PLOT_DRAG_XRANGE");
    ImPlot::SetupLock();

    const auto yrange = ImPlot::GetPlotLimits().Y;
    const ImVec2 px_min = ImPlot::PlotToPixels(*xmin, yrange.Max);
    const ImVec2 px_max = ImPlot::PlotToPixels(*xmax, yrange.Min);

    bool move = !ImHasFlag(flags, DragXRectFlag::NoMove);
    bool resize = !ImHasFlag(flags, DragXRectFlag::NoResize);
    const bool cursor = !ImHasFlag(flags, DragXRectFlag::NoCursor);
    bool clicked = false;
    bool hovered = false;
    bool held = false;
    bool modified = false;

    const ImGuiID id = ImGui::GetCurrentWindow()->GetID(caller_id);

    if (move) {
        const double xdelta = move_xrange(id, px_min, px_max, clicked, hovered, held);
        if ((hovered || held) && cursor) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        if (xdelta) {
            *xmin += xdelta;
            *xmax += xdelta;
            modified = true;
        }
    }

    // if (resize) {
    //     modified = resize_rect(id + 1, xmin, xmax, px_min, px_max, clicked, hovered, held);
    //     if ((hovered || held) && cursor) {
    //         ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    //     }
    // }

    draw_plot_vspan(*xmin, *xmax, color);

    ImGui::PopID();
    if (out_clicked) {
        *out_clicked = clicked;
    }
    if (out_hovered) {
        *out_hovered = hovered;
    }
    if (out_held) {
        *out_held = held;
    }
    return modified;
}

bool drag_xrange(
    int id,
    ImPlotRange *xrange,
    const ImColor& color,
    plot::DragXRectFlags flag,
    bool *out_clicked,
    bool *out_hovered,
    bool *held
)
{
    return drag_xrange(id, &xrange->Min, &xrange->Max, color, flag, out_clicked, out_hovered, held);
}

}; // namespace plot
