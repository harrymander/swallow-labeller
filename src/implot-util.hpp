#ifndef RECAP_LABELLER_IMPLOT_UTIL_HPP_INCLUDE
#define RECAP_LABELLER_IMPLOT_UTIL_HPP_INCLUDE

#include <imgui.h>
#include <implot.h>

namespace recap::implot_util {

/**
 * Draw a rectangle between x0 and x1.
 *
 * @param x0 The left edge of the rectangle in plot coordinates
 * @param x1 The left edge of the rectangle in plot coordinates
 * @param color Color of the rectangle to draw
 * @param height_px Height of the rectangle in pixels; if negative, draw from the bottom of plot. If
 *     0, span the entire height of the plot.
 */
void plot_vspan(double x0, double x1, const ImColor& color, float height_px = 0);

inline void plot_vspan(const ImPlotRange& range, const ImColor& color, float height_px = 0)
{
    plot_vspan(range.Min, range.Max, color, height_px);
}

}; // namespace recap::implot_util

#endif // RECAP_LABELLER_IMPLOT_UTIL_HPP_INCLUDE
