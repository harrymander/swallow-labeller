#ifndef INCLUDE_PLOT_DRAG_RECT_HPP
#define INCLUDE_PLOT_DRAG_RECT_HPP

#include "imgui.h"
#include "implot.h"

namespace plot {

typedef unsigned int DragXRectFlags;

enum DragXRectFlag {
    // Disable cursors on move and resize
    NoCursor = 1 << 0,

    // Disable moving
    NoMove = 1 << 1,

    // Disable resize
    NoResize = 1 << 2,

    NoInteraction = NoMove | NoResize,
};

bool drag_xrange(
    int id,
    double *xmin,
    double *xmax,
    const ImColor& color,
    plot::DragXRectFlags flags = 0,
    bool *clicked = nullptr,
    bool *hovered = nullptr,
    bool *held = nullptr
);

bool drag_xrange(
    int id,
    ImPlotRange *xrange,
    const ImColor& color,
    plot::DragXRectFlags flags = 0,
    bool *clicked = nullptr,
    bool *hovered = nullptr,
    bool *held = nullptr
);

} // namespace plot

#endif // INCLUDE_PLOT_DRAG_RECT_HPP
