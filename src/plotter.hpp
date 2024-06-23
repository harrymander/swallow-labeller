#ifndef INCLUDE_RECAP_LABELLER_PLOTTER_HPP
#define INCLUDE_RECAP_LABELLER_PLOTTER_HPP

#include "data.hpp"
#include "implot.h"
#include "labelling-task.hpp"
#include "selector.hpp"

#include "imgui.h"

#include <cmath>
#include <vector>

namespace recap::labeller::plotter {

class SwallowAnnotationEditor {
public:
    explicit SwallowAnnotationEditor(labelling_task::SwallowAnnotation annotation = {});

    void draw_swallow_label_info();
    void draw_swallow_apnea_info();
    void draw_swallow_notes();

    void draw_apnea_selection();

    void draw_earclick_label_info();
    void draw_earclick_notes();

private:
    static constexpr ImColor ApneaLabelColor = ImColor(1.0F, 1.0F, 0.0F, 0.2F);

    [[nodiscard]] bool draw_apnea_selector();

    ImPlotRange apnea_range = {NAN, NAN};
    plot::PlotXSelector apnea_selector;

    std::vector<ImPlotRange> earclick_ranges = {};
    plot::PlotXSelector earclick_selector;

    labelling_task::SwallowAnnotation annotation;
    labelling_task::SRCPattern src_pattern;
    bool is_ambiguous;
    std::string swallow_notes;
    std::string ear_click_notes;
};

class SwallowTaskPlotter {
public:
    explicit SwallowTaskPlotter(plot::SwallowTaskData data);
    void draw(const char *id);

private:
    void draw_audio_plot();
    void draw_flow_plot();
    void draw_summary_plot();

    void
    plot_data(const char *, const std::vector<double>&, const std::vector<double>&, const char *);

    void plot_audio_line();
    void plot_flow_line();
    void plot_event_digital() const;

    plot::PlotXSelector summary_selector = {};
    plot::PlotXSelector selector = {};
    ImPlotRange selector_range = {};
    ImPlotRange last_selector_range = {NAN, NAN};

    plot::SwallowTaskData data;
    std::vector<double> event;
    ImPlotRange summary_range;
    SwallowAnnotationEditor annotation_editor;
};

}; // namespace recap::labeller::plotter

#endif // INCLUDE_RECAP_LABELLER_PLOTTER_HPP
