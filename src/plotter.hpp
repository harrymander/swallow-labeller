#ifndef INCLUDE_RECAP_LABELLER_PLOTTER_HPP
#define INCLUDE_RECAP_LABELLER_PLOTTER_HPP

#include "data.hpp"
#include "implot.h"
#include "selector.hpp"

#include <cmath>
#include <vector>

namespace recap::labeller::plotter {

class SwallowTaskPlotter {
public:
    explicit SwallowTaskPlotter(const plot::SwallowTaskData& data);
    void draw(const char *id);

private:
    void draw_audio_plot();
    void draw_flow_plot();
    void draw_summary_plot();

    void plot_data(const std::vector<double>& time, const std::vector<double>& data);

    void plot_audio_line();
    void plot_flow_line();
    void plot_event_digital() const;

    plot::SwallowTaskData data;
    std::vector<double> event;
    ImPlotRange summary_range;

    plot::PlotXSelector summary_selector;
    plot::PlotXSelector selector;
    ImPlotRange selector_range;
    ImPlotRange last_selector_range = {NAN, NAN};
};

}; // namespace recap::labeller::plotter

#endif // INCLUDE_RECAP_LABELLER_PLOTTER_HPP
