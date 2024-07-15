#ifndef RECAP_LABELLER_GUI_HPP
#define RECAP_LABELLER_GUI_HPP

#include "../app.hpp"
#include "imgui.h"
#include "widgets/color-scheme-selector.hpp"
#include "widgets/plot-range-dragger.hpp"
#include "widgets/plot-range-selector.hpp"
#include "widgets/plot-range.hpp"

#include <fmt/core.h>

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

        static bool begin(const char *id);
        void plot_data(
            const std::vector<double>& x, const std::vector<double>& y, const SwallowTaskData& data
        );
        static void end();

    private:
        std::string m_ylabel;
        fmt::format_string<double> m_cursor_format;
        widgets::PlotRange& m_xrange;
        widgets::PlotRangeSelector m_delta_selector;
    };

    bool m_first_draw = true;
    std::optional<std::string> m_ini_path = std::nullopt;
    bool m_stop_requested = false;
    bool m_ready_to_stop = false;
    bool m_show_imgui_demo_window = false;
    bool m_show_implot_demo_window = false;
    bool m_show_imgui_metrics = false;
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

    void setup_dockspace() const;
    void draw_active_task(recap::labeller::app::ActiveSwallowLabellingTask& active_task);
    void draw_main_window();
    void draw_task_list();
    void draw_menu_bar();
    static void draw_debug_info();

    void draw_plots(const SwallowTaskData& data);
    void draw_plot_summary_selector();
    void on_new_active_task(const app::App::ActiveTaskVariant& new_variant);
};

}; // namespace recap::labeller::gui

#endif //  RECAP_LABELLER_GUI_HPP
