#ifndef INCLUDE_PLOT_DRAGGER_HPP
#define INCLUDE_PLOT_DRAGGER_HPP

#include "implot.h"

namespace plot {

typedef int PlotRangeDraggerFlags;

class PlotRangeDragger {
public:
    void draw_update(ImPlotRange& range, PlotRangeDraggerFlags = 0);

    enum {
        // Disable moving/creating range by clicking and dragging outside existing range
        NoCreate = 1 << 0,

        // Disable resizing range by clicking and dragging edges
        NoResize = 1 << 1,

        // Disable moving range by clicking and dragging in centre
        NoMove = 1 << 2,

        // Disable all interactions
        Disable = NoCreate | NoResize | NoMove,
    };

private:
    enum class State {
        None,
        MouseOutside,
        DragCreate,
        Moving,
        MinResizing,
        MaxResizing,
    };

    State state = State::None;
    ImPlotRange xrange_dragstart;
    double xmouse_dragstart = 0;

    void draw_cursor() const;
    void handle_mouse_up(const ImPlotRange&, double, ImPlotAxisFlags);
    void handle_mouse_down(ImPlotRange&, double, ImPlotAxisFlags);
    void handle_move(ImPlotRange&, double);
    void min_resize(ImPlotRange&, double);
    void max_resize(ImPlotRange&, double);
};

}; // namespace plot

#endif // INCLUDE_PLOT_DRAGGER_HPP
