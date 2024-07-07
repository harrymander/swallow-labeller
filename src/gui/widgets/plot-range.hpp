#ifndef RECAP_LABELLER_GUI_WIDGETS_PLOT_RANGE_HPP
#define RECAP_LABELLER_GUI_WIDGETS_PLOT_RANGE_HPP

#include <imgui.h>

namespace recap::labeller::gui::widgets {

struct PlotRange {
    double start;
    double end;

    [[nodiscard]] double range() const { return end - start; }
};

/**
 * Draw a rectangle between x0 and x1.
 *
 * @param x0 The left edge of the rectangle in plot coordinates
 * @param x1 The left edge of the rectangle in plot coordinates
 * @param color Color of the rectangle to draw
 * @param height_px Height of the rectangle in pixels; if negative, draw from the bottom of plot. If
 *     0, span the entire height of the plot.
 */
void draw_plot_range(double x0, double x1, const ImColor& color, float height_px = 0);

inline void draw_plot_range(const PlotRange& range, const ImColor& color, float height_px = 0)
{
    draw_plot_range(range.start, range.end, color, height_px);
}

}; // namespace recap::labeller::gui::widgets

#endif // RECAP_LABELLER_GUI_WIDGETS_PLOT_RANGE_HPP
