#include "dragger.hpp"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cmath>

namespace plot {

static inline bool isnear(double a, double b, double eps)
{
    return std::abs(a - b) <= eps;
}

static inline bool key_down_or_none(ImGuiKey key)
{
    return key == ImGuiKey_None || ImGui::IsKeyDown(key);
}

void PlotRangeDragger::draw_update(
    ImPlotRange& range, PlotRangeDraggerFlags flags, ImGuiKey create_key
)
{
    const double xmouse = ImPlot::GetPlotMousePos().x;
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        handle_mouse_down(range, xmouse, flags, create_key);
    } else {
        handle_mouse_up(range, xmouse, flags);
    }

    draw_cursor();
}

void PlotRangeDragger::draw_cursor() const
{
    using enum State;
    switch (state) {
    case Moving:
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        break;
    case MinResizing:
    case MaxResizing:
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        break;
    case MouseOutside:
    case DragCreate:
    case None:
        break;
    }
}

void PlotRangeDragger::handle_mouse_down(
    ImPlotRange& xrange, double xmouse, PlotRangeDraggerFlags flags, ImGuiKey create_key
)
{
    switch (state) {
        using enum State;
    case MouseOutside:
        if (!(flags & NoCreate) && key_down_or_none(create_key)) {
            xmouse_dragstart = xmouse;
            state = DragCreate;
        }
        break;
    case DragCreate:
        if (xmouse > xmouse_dragstart) {
            xrange.Min = xmouse_dragstart;
            xrange.Max = xmouse;
            state = MaxResizing;
        } else if (xmouse < xmouse_dragstart) {
            xrange.Min = xmouse;
            xrange.Max = xmouse_dragstart;
            state = MinResizing;
        }
        break;
    case Moving:
        handle_move(xrange, xmouse);
        break;
    case MinResizing:
        min_resize(xrange, xmouse);
        break;
    case MaxResizing:
        max_resize(xrange, xmouse);
        break;
    case None:
        break;
    }
}

void PlotRangeDragger::handle_mouse_up(
    const ImPlotRange& range, double xmouse, PlotRangeDraggerFlags flags
)
{
    using enum State;
    if (ImPlot::IsPlotHovered()) {
        const double mouse_near = ImPlot::PixelsToPlot(20, 0).x;
        if (isnear(xmouse, range.Min, mouse_near)) {
            state = (flags & NoResize) ? None : MinResizing;
        } else if (isnear(xmouse, range.Max, mouse_near)) {
            state = (flags & NoResize) ? None : MaxResizing;
        } else if (xmouse > range.Min && xmouse < range.Max) {
            if (flags & NoMove) {
                state = None;
            } else {
                state = Moving;
                xrange_dragstart = range;
                xmouse_dragstart = xmouse;
            }
        } else {
            state = MouseOutside;
        }
    } else {
        state = None;
    }
}

void PlotRangeDragger::min_resize(ImPlotRange& xrange, double xmouse)
{
    xrange.Min = ImPlot::GetPlotLimits().X.Clamp(xmouse);
    if (xrange.Min > xrange.Max) {
        state = State::MaxResizing;
        std::swap(xrange.Min, xrange.Max);
    }
}

void PlotRangeDragger::max_resize(ImPlotRange& xrange, double xmouse)
{
    xrange.Max = ImPlot::GetPlotLimits().X.Clamp(xmouse);
    if (xrange.Min > xrange.Max) {
        state = State::MinResizing;
        std::swap(xrange.Min, xrange.Max);
    }
}

void PlotRangeDragger::handle_move(ImPlotRange& xrange, double xmouse)
{
    const double dx = xmouse - xmouse_dragstart;
    const auto xlim = ImPlot::GetPlotLimits().X;

    if (!xlim.Contains(xrange_dragstart.Min + dx)) {
        xrange.Max = xlim.Min + xrange.Size();
        xrange.Min = xlim.Min;
    } else if (!xlim.Contains(xrange_dragstart.Max + dx)) {
        xrange.Min = xlim.Max - xrange.Size();
        xrange.Max = xlim.Max;
    } else {
        xrange.Min = xrange_dragstart.Min + dx;
        xrange.Max = xrange_dragstart.Max + dx;
    }
}

}; // namespace plot
