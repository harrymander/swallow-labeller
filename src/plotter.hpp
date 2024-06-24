#ifndef INCLUDE_RECAP_LABELLER_PLOTTER_HPP
#define INCLUDE_RECAP_LABELLER_PLOTTER_HPP

#include "data.hpp"
#include "labelling-task.hpp"
#include "selector.hpp"

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
        recap::labeller::task::SwallowAnnotation annotation = {}
    );
    ~SwallowTaskPlotter() noexcept;
    SwallowTaskPlotter(SwallowTaskPlotter&&) noexcept;
    SwallowTaskPlotter& operator=(SwallowTaskPlotter&&) noexcept;

    SwallowTaskPlotter(const SwallowTaskPlotter&) noexcept = delete;
    SwallowTaskPlotter& operator=(const SwallowTaskPlotter&) noexcept = delete;

    void draw(const char *id);

    [[nodiscard]] const recap::labeller::task::SwallowAnnotation& annotation() const;

    [[nodiscard]] bool valid_annotation() const;

private:
    static ImPlotRange
    initial_range(const std::vector<double>& time, const std::vector<uint8_t>& event);

    void draw_plots();
    void draw_audio_plot();
    void draw_flow_plot();
    void draw_summary_plot();

    void
    plot_data(const char *, const std::vector<double>&, const std::vector<double>&, const char *);

    void plot_audio_line();
    void plot_flow_line();
    void plot_event_digital() const;

    recap::plot::PlotXSelector summary_selector = {};
    recap::plot::PlotXSelector selector = {};
    ImPlotRange selector_range = {};
    ImPlotRange last_selector_range = {NAN, NAN};

    class AnnotationEditor;

    recap::labeller::data::SwallowTaskData data;
    std::vector<double> event;
    ImPlotRange summary_range;
    std::unique_ptr<AnnotationEditor> annotation_editor;
};

}; // namespace recap::labeller::plotter

#endif // INCLUDE_RECAP_LABELLER_PLOTTER_HPP
