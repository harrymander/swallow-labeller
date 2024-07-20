#ifndef RECAP_LABELLER_GUI_HPP
#define RECAP_LABELLER_GUI_HPP

#include "app/app.hpp"
#include "gui/widgets/color-scheme-selector.hpp"
#include "gui/widgets/plot-range-dragger.hpp"
#include "gui/widgets/plot-range-selector.hpp"
#include "gui/widgets/plot-range.hpp"

#include <fmt/core.h>
#include <imgui.h>

#include <optional>
#include <string>

namespace recap::labeller::gui {

class Gui {
public:
    explicit Gui(recap::labeller::app::App& app);
    ~Gui();

    Gui(const Gui&) = delete;
    Gui operator=(const Gui&) = delete;
    Gui(Gui&&) = delete;
    Gui operator=(Gui&&) = delete;

    void draw();
    void stop();
    static void set_scaling_factor(float scaling_factor);

    [[nodiscard]] bool ready_to_stop() const { return m_ready_to_stop; }

private:
    class Plotter {
    public:
        Plotter(
            std::string ylabel, fmt::format_string<double> cursor_format, widgets::PlotRange& xrange
        );

        template <typename DrawFunc> void draw(const char *id, float height, DrawFunc&& draw);

        void plot_data(
            const std::vector<double>& x, const std::vector<double>& y, const SwallowTaskData& data
        );

    private:
        std::string m_ylabel;
        fmt::format_string<double> m_cursor_format;
        widgets::PlotRange& m_xrange;
        widgets::PlotRangeSelector m_delta_selector;
    };

    bool m_first_draw = true;
    std::string m_ini_path;
    bool m_stop_requested = false;
    bool m_ready_to_stop = false;
    bool m_show_imgui_demo_window = false;
    bool m_show_implot_demo_window = false;
    bool m_show_imgui_metrics = false;
    bool m_critical_error_modal_open = false;
    recap::labeller::gui::widgets::ColorSchemeSelector m_color_scheme_selector;

#if NDEBUG
    bool m_show_debug_info = false;
#else
    bool m_show_debug_info = true;
#endif

    bool m_only_show_annotated_tasks = false;
    ImGuiTextFilter m_task_list_text_filter;

    widgets::PlotRangeDragger m_plot_summary_dragger;
    widgets::PlotRangeSelector m_plot_summary_selector;
    widgets::PlotRange m_plot_summary_range = {NAN, NAN};

    recap::labeller::app::App& m_app;
    recap::labeller::app::App::NewActiveTaskObservable::Observer m_new_active_task_observer;
    Plotter m_flow_plotter;
    Plotter m_audio_plotter;

    struct Annotator {
        widgets::PlotRangeSelector apnea_range_selector;
        widgets::PlotRange apnea_temp_range = {NAN, NAN};
        widgets::PlotRangeDragger apnea_range_dragger;
        widgets::PlotRangeSelector earclick_range_selector;
        widgets::PlotRangeDragger earclick_range_dragger;
        widgets::PlotRange earclick_temp_range = {NAN, NAN};
        std::optional<app::EarClickLabel::ID> selected_ear_click_id = std::nullopt;
        std::optional<app::EarClickLabel::ID> hovered_ear_click_id = std::nullopt;

        void reset_ear_click();

        Annotator() = default;
        explicit Annotator(const app::ActiveSwallowLabellingTaskView& task_view);
    };

    Annotator m_annotator;

    void setup_imgui_ini();
    void setup_dockspace() const;
    void draw_critical_error(const std::string& error);
    void draw_main_window();
    void draw_task_list();
    void draw_menu_bar();
    static void draw_debug_info();

    void draw_plots(app::ActiveSwallowLabellingTaskView& task_view);
    void draw_flow_plot(app::ActiveSwallowLabellingTaskView& task_view);
    void draw_apnea_label_region(
        const app::ActiveSwallowLabellingTaskView& task_view,
        float height = 0,
        bool selected_color = false
    ) const;
    void draw_audio_plot(app::ActiveSwallowLabellingTaskView& task_view);
    void draw_plot_summary_selector();
    void on_new_active_task(const app::App::ActiveTaskVariant& new_task);

    void draw_annotation_submit(app::ActiveSwallowLabellingTaskView& task_view);
    void draw_label_editor(app::ActiveSwallowLabellingTaskView& task_view);
    void draw_earclick_labels_listbox(
        app::ActiveSwallowLabellingTaskView& task_view,
        const std::vector<app::EarClickLabel>& labels
    );
    void draw_earclick_label_regions(
        const app::ActiveSwallowLabellingTaskView& task_view,
        float height = 0,
        bool selected_color = false
    ) const;
};

}; // namespace recap::labeller::gui

#endif //  RECAP_LABELLER_GUI_HPP
