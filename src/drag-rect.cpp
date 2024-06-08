#include "drag-rect.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "implot_internal.h"

#include <algorithm>

namespace plot {

constexpr float EdgeWidth = 20;

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

    bool clicked = false;
    bool hovered = false;
    bool held = false;
    bool modified = false;
    ImGuiID id = ImGui::GetCurrentWindow()->GetID(caller_id);

    auto button_behaviour = [&](const ImRect& rect) {
        ImGui::KeepAliveID(id);
        clicked = ImGui::ButtonBehavior(rect, id, &hovered, &held);
        id += 1;
    };

    const bool show_cursor = !ImHasFlag(flags, DragXRectFlag::NoCursor);
    if (!ImHasFlag(flags, DragXRectFlag::NoInput)) {
        auto move_rect = [&](const ImRect& rect) -> float {
            button_behaviour(rect);
            if (held && ImGui::IsMouseDragging(0)) {
                return ImGui::GetIO().MouseDelta.x;
            }
            return 0.;
        };

        auto move_edge = [&](const ImVec2& edge) -> float {
            const float ret =
                move_rect({edge.x - EdgeWidth / 2, px_min.y, edge.x + EdgeWidth / 2, px_max.y});
            if ((held || hovered) && show_cursor) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
            return ret;
        };

        const ImPlotRange& constraint = current_plot->XAxis(0).ConstraintRange;
        modified = true;
        float delta =
            move_rect({px_min.x + EdgeWidth / 2, px_min.y, px_max.x - EdgeWidth / 2, px_max.y});
        if (delta) {
            if ((held || hovered) && show_cursor) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            px_min.x += delta;
            px_max.x += delta;
            xmin = ImPlot::PixelsToPlot(px_min).x;
            xmax = ImPlot::PixelsToPlot(px_max).x;
            if (xmin < constraint.Min) {
                xmax = constraint.Min + (xmax - xmin);
                xmin = constraint.Min;
            } else if (xmax > constraint.Max) {
                xmin = constraint.Max - (xmax - xmin);
                xmax = constraint.Max;
            }
        } else if ((delta = move_edge(px_min))) {
            px_min.x += delta;
            xmin = std::max(constraint.Min, ImPlot::PixelsToPlot(px_min).x);
        } else if ((delta = move_edge(px_max))) {
            px_max.x += delta;
            xmax = std::min(constraint.Max, ImPlot::PixelsToPlot(px_max).x);
        } else {
            modified = false;
        }
    } else {
        button_behaviour({px_min, px_max});
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
