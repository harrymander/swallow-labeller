#ifndef INCLUDE_RECAP_LABELLER_PLOTTER_HPP
#define INCLUDE_RECAP_LABELLER_PLOTTER_HPP

#include "data.hpp"
#include "gui/widgets/plot-range-dragger.hpp"
#include "gui/widgets/plot-range-selector.hpp"
#include "gui/widgets/plot-range.hpp"
#include "labelling-task.hpp"

#include <fmt/core.h>
#include <imgui.h>
#include <implot.h>

#include <cmath>
#include <memory>
#include <vector>

namespace recap::labeller::plotter {

class SwallowTaskPlotter {
public:
    explicit SwallowTaskPlotter(
        recap::labeller::data::SwallowTaskData data,
        recap::labeller::SwallowAnnotation annotation = {}
    );
    ~SwallowTaskPlotter() noexcept;
    SwallowTaskPlotter(SwallowTaskPlotter&&) noexcept;
    SwallowTaskPlotter& operator=(SwallowTaskPlotter&&) noexcept;

    SwallowTaskPlotter(const SwallowTaskPlotter&) noexcept = delete;
    SwallowTaskPlotter& operator=(const SwallowTaskPlotter&) noexcept = delete;

    void draw(const char *id);

    [[nodiscard]] const recap::labeller::SwallowAnnotation& annotation() const;

    void reset_annotation();

    [[nodiscard]] bool valid_annotation() const;

private:
    static recap::labeller::gui::widgets::PlotRange
    initial_range(const std::vector<double>& time, const std::vector<uint8_t>& event);

    void draw_plots();
    void draw_audio_plot();
    void draw_flow_plot();
    void draw_summary_plot();

    void plot_data(
        const char *id,
        const std::vector<double>& x,
        const std::vector<double>& y,
        const char *ylabel,
        fmt::format_string<double> yfmt
    );

    void plot_audio_line();
    void plot_flow_line();
    void plot_event_digital() const;

    recap::labeller::gui::widgets::PlotRangeSelector summary_selector = {};
    recap::labeller::gui::widgets::PlotRange selector = {};
    recap::labeller::gui::widgets::PlotRange selector_range = {};
    recap::labeller::gui::widgets::PlotRange last_selector_range = {NAN, NAN};
    recap::labeller::gui::widgets::PlotRange flow_time_delta_range = {};
    recap::labeller::gui::widgets::PlotRangeSelector flow_time_delta_selector;
    recap::labeller::gui::widgets::PlotRangeDragger summary_dragger;

    class AnnotationEditor;

    recap::labeller::data::SwallowTaskData data;
    std::vector<double> event;
    recap::labeller::gui::widgets::PlotRange summary_range;
    std::unique_ptr<AnnotationEditor> annotation_editor;
};

}; // namespace recap::labeller::plotter

#endif // INCLUDE_RECAP_LABELLER_PLOTTER_HPP
