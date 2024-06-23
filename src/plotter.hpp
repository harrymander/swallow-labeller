#ifndef INCLUDE_RECAP_LABELLER_PLOTTER_HPP
#define INCLUDE_RECAP_LABELLER_PLOTTER_HPP

#include "data.hpp"
#include "labelling-task.hpp"
#include "selector.hpp"

#include <imgui.h>
#include <implot.h>

#include <cmath>
#include <optional>
#include <vector>

namespace recap::labeller::plotter {

class PlotSelectionsEditor {
public:
    PlotSelectionsEditor(
        std::string_view name,
        ImColor color,
        ImColor hovered_color,
        ImColor selected_color,
        const std::vector<labelling_task::TimeRange>& ranges = {}
    );

    void draw_plot_selection(const char *id);
    void draw_list(const char *id);

    [[nodiscard]] bool has_selections() const { return !ranges.empty(); }

private:
    std::string name;
    ImColor color;
    ImColor hovered_color;
    ImColor selected_color;
    std::vector<ImPlotRange> ranges;
    std::vector<std::string> labels;

    plot::PlotXSelector selector;
    ImPlotRange next_range = {NAN, NAN};
    std::optional<std::size_t> selected_index = std::nullopt;
    std::optional<std::size_t> hovered_index = std::nullopt;

    void remove_selection(std::size_t i);
    void set_labels();
};

class SwallowAnnotationEditor {
public:
    explicit SwallowAnnotationEditor(labelling_task::SwallowAnnotation annotation = {});

    void draw_swallow_label_info();
    void draw_swallow_apnea_info();
    void draw_swallow_notes();

    void draw_apnea_selection();

    void draw_earclick_label_info();
    void draw_earclick_notes();
    void draw_earclick_selection();
    void draw_earclick_selection_list();

    [[nodiscard]] bool has_earclick_selections() const
    {
        return earclick_selections.has_selections();
    }

private:
    static constexpr ImColor ApneaLabelColor = ImColor(1.0F, 1.0F, 0.0F, 0.2F);
    static constexpr ImColor EarclickLabelColor = ImColor(0.0F, 1.0F, 0.0F, 0.1F);
    static constexpr ImColor EarclickLabelColorHovered = ImColor(0.0F, 1.0F, 0.0F, 0.25F);
    static constexpr ImColor EarclickLabelColorSelected = ImColor(0.0F, 1.0F, 0.0F, 0.4F);

    [[nodiscard]] bool draw_apnea_selector();

    ImPlotRange apnea_range = {NAN, NAN};
    plot::PlotXSelector apnea_selector;

    labelling_task::SwallowAnnotation annotation;
    labelling_task::SRCPattern src_pattern;
    bool is_ambiguous;
    std::string swallow_notes;
    std::string ear_click_notes;
    PlotSelectionsEditor earclick_selections;
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
