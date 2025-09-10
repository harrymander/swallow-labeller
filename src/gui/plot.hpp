#ifndef RECAP_LABELLER_GUI_PLOT_HPP_INCLUDE
#define RECAP_LABELLER_GUI_PLOT_HPP_INCLUDE

#include "gui/widgets/plot-range-selector.hpp"
#include "gui/widgets/plot-range.hpp"

#include <fmt/core.h>

#include <string>
#include <vector>

namespace recap::labeller::gui {

// Represents a plot for a single channel of data with hover tooltip and right-click range selector.
// View is locked to xrange.
class Plot {
public:
    Plot(
        const std::vector<double>& time,
        const std::vector<double>& y,
        widgets::PlotRange& xrange,
        std::string ylabel,
        fmt::format_string<double> cursor_format
    ) :
        m_xdata(time),
        m_ydata(y),
        m_xrange(xrange),
        m_ylabel(std::move(ylabel)),
        m_cursor_format(cursor_format)
    {}

    // Must be called inside PlotBegin/PlotEnd.
    // Returns number of points plotted.
    std::size_t draw();

private:
    widgets::PlotRangeSelector m_delta_selector;

    const std::vector<double>& m_xdata;
    const std::vector<double>& m_ydata;
    widgets::PlotRange& m_xrange;
    std::string m_ylabel;
    fmt::format_string<double> m_cursor_format;
};

// Must be called inside PlotBegin/End

}; // namespace recap::labeller::gui

#endif // RECAP_LABELLER_GUI_PLOT_HPP_INCLUDE
