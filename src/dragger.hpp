#ifndef INCLUDE_PLOT_DRAGGER_HPP
#define INCLUDE_PLOT_DRAGGER_HPP

#include "implot.h"

namespace plot {

class PlotRangeDragger {
public:
    void draw_update(ImPlotRange& range);

private:
    enum class State {
        None,
        Hovered,
        DragCreate,
        Dragging,
        MinResizing,
        MaxResizing,
    };

    State state = State::None;
    ImPlotRange xrange_dragstart;
    double xmouse_dragstart = 0;

    void draw_cursor() const;
    void check_mouse(const ImPlotRange&, double);
    void handle_mouse_down(ImPlotRange&, double);
    void handle_drag(ImPlotRange&, double);
    void min_resize(ImPlotRange&, double);
    void max_resize(ImPlotRange&, double);
};

}; // namespace plot

#endif // INCLUDE_PLOT_DRAGGER_HPP
