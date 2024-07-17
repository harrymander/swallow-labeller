#define IMGUI_DEFINE_MATH_OPERATORS

#include "gui.hpp"

#include "app/app.hpp"
#include "gui/font.hpp"
#include "gui/widgets/plot-range-selector.hpp"
#include "gui/widgets/plot-range.hpp"
#include "gui/widgets/radio-button-enum.hpp"
#include "gui/widgets/util.hpp"
#include "models/time-range.hpp"
#include "util/util.hpp"
#include "util/variant-visitor.hpp"

#include <IconsFontAwesome6.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <variant>

#define FILE_ERR_ICON ICON_FA_FILE_CIRCLE_EXCLAMATION
#define ERR_ICON ICON_FA_TRIANGLE_EXCLAMATION
#define DEBUG_INFO_ICON ICON_FA_GEAR
#define ANNOTATED_TASK_ICON ICON_FA_SQUARE_CHECK
#define HINT_ICON ICON_FA_LIGHTBULB
#define ICON_TEXT_SPACE "  "

namespace recap::labeller::gui {

Gui::Gui(app::App& app) :
    m_app(app),
    m_new_active_task_observer(m_app.subscribe_new_active_task([this](const auto& v) {
        on_new_active_task(v);
    })),
    m_flow_plotter("Flow (L/min)", "{:g} L/min", m_plot_summary_range),
    m_audio_plotter("Ear audio (V)", "{:g} V", m_plot_summary_range)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    setup_imgui_ini();

    setup_fonts();

    on_new_active_task(m_app.active_task_view());
}

namespace {

bool is_valid_ini_path(const std::filesystem::path& path)
{
    namespace fs = std::filesystem;
    if (fs::is_directory(path)) {
        spdlog::error("Invalid INI path: '{}' is a directory", path);
        return false;
    }

    const auto parent = path.parent_path();
    if (!parent.empty() && !fs::is_directory(parent)) {
        spdlog::error(
            "Invalid INI path: parent directory '{}' does not exist or is not a directory", parent
        );
        return false;
    }

    return true;
}

}; // namespace

void Gui::setup_imgui_ini()
{
    ImGuiIO& io = ImGui::GetIO();
    const char *const env = std::getenv("RECAP_LABELLER_IMGUI_INI_PATH");
    if (env) {
        if (*env) {
            std::filesystem::path path(env);
            if (is_valid_ini_path(path)) {
                m_ini_path = path.make_preferred().string();
                spdlog::info("Custom ImGui INI path: '{}'", m_ini_path);
                io.IniFilename = m_ini_path.c_str();
            } else {
                spdlog::error("Using default ImGui INI path");
            }
        } else {
            spdlog::info("RECAP_LABELLER_IMGUI_INI_PATH empty: disabling INI file");
            io.IniFilename = nullptr;
        }
    } else {
        spdlog::debug("RECAP_LABELLER_IMGUI_INI_PATH not set, using default ImGui INI path");
    }
}

Gui::~Gui()
{
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void Gui::stop()
{
    spdlog::info("GUI close requested");
    m_stop_requested = true;
    m_ready_to_stop = true;
}

static const char *const TaskListWindowId = "##tasklistwindow";
static const char *const MainWindowId = "##mainwindow";
static const char *const LabelInfoWindowId = "##labelinfowindow";

namespace {

template <typename DrawFunc> void draw_window(const char *id, DrawFunc&& draw)
{
    constexpr ImGuiWindowFlags WindowFlags =
        (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove
         | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

    if (ImGui::Begin(id, nullptr, WindowFlags)) {
        draw();
    }
    ImGui::End();
}

template <typename ShowFunc> void show_window(bool& open, ShowFunc&& show)
{
    if (open) {
        show(&open);
    }
}

}; // namespace

void Gui::draw()
{
    if (ImGui::BeginMainMenuBar()) {
        draw_menu_bar();
        ImGui::EndMainMenuBar();
    }
    setup_dockspace();

    draw_window(TaskListWindowId, [this]() { draw_task_list(); });
    draw_window(MainWindowId, [this]() { draw_main_window(); });

    auto *task_view = std::get_if<app::ActiveSwallowLabellingTaskView>(&m_app.active_task_view());
    if (task_view != nullptr) {
        draw_window(LabelInfoWindowId, [this, task_view]() { draw_label_editor(*task_view); });
    }

    show_window(m_show_imgui_demo_window, ImGui::ShowDemoWindow);
    show_window(m_show_imgui_metrics, ImGui::ShowMetricsWindow);
    show_window(m_show_implot_demo_window, ImPlot::ShowDemoWindow);

    m_first_draw = false;
}

void Gui::setup_dockspace() const
{
    constexpr ImGuiDockNodeFlags DockspaceFlags =
        (ImGuiDockNodeFlags_NoUndocking | ImGuiDockNodeFlags_AutoHideTabBar
         | ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoTabBar);

    // Initial widths for sidebars from which we calculate dock node ratios - these are just
    // approximate sizes since the ratio calculations don't factor in window spacing etc.
    constexpr float TasklistPx = 250;
    constexpr float LabelInfoPx = 350;
    constexpr float MinRatio = 0.1;
    constexpr float MaxRatio = 0.25;

    // If the dockspace ID already exists, the the node sizes are already set in imgui.ini. The
    // following is adapted from:
    // https://gist.github.com/AidanSun05/953f1048ffe5699800d2c92b88c36d9f
    ImGuiID id = ImGui::GetID("##dockspace");
    const ImGuiViewport *const viewport = ImGui::GetMainViewport();
    if (m_first_draw) [[unlikely]] {
        const bool is_configured = ImGui::DockBuilderGetNode(id) == nullptr;
        ImGui::DockSpaceOverViewport(id, viewport, DockspaceFlags);
        if (is_configured) {
            spdlog::debug("Setting up dockspace");
            ImGui::DockBuilderRemoveNode(id);
            ImGui::DockBuilderAddNode(id);

            ImGuiID dock_tasklist;
            ImGuiID dock_main;
            ImGuiID dock_label_info;
            const float viewport_width = viewport->Size.x;
            ImGui::DockBuilderSplitNode(
                id,
                ImGuiDir_Left,
                std::clamp(TasklistPx / viewport_width, MinRatio, MaxRatio),
                &dock_tasklist,
                &dock_main
            );
            ImGui::DockBuilderSplitNode(
                dock_main,
                ImGuiDir_Right,
                std::clamp(LabelInfoPx / (viewport_width - TasklistPx), MinRatio, MaxRatio),
                &dock_label_info,
                &dock_main
            );

            ImGui::DockBuilderDockWindow(TaskListWindowId, dock_tasklist);
            ImGui::DockBuilderDockWindow(MainWindowId, dock_main);
            ImGui::DockBuilderDockWindow(LabelInfoWindowId, dock_label_info);
            ImGui::DockBuilderFinish(id);
        } else {
            spdlog::debug("Not setting up dockspace since sizes already set in imgui.ini");
        }
    } else {
        ImGui::DockSpaceOverViewport(id, viewport, DockspaceFlags);
    }
}

void Gui::draw_main_window()
{
    if (m_show_debug_info) {
        draw_debug_info();
    }

    const auto& task = m_app.tasks()[m_app.active_task_index()];
    const auto& info = task.info();
    ImGui::Text(
        "Subject #%u, %s swallows, repeat #%u, swallow #%u (%s)",
        info.subject,
        swallow_test_type_string(info.test_type).c_str(),
        info.repeatnum,
        info.swallownum,
        task.data_path().c_str()
    );

    VariantVisitor{
        [this](app::ActiveSwallowLabellingTaskView& task) { draw_plots(task); },
        [this](const app::ActiveSwallowLabellingTaskErrorView& error) {
            ImGui::Text(ERR_ICON ICON_TEXT_SPACE "%s", error.error_msg().c_str());
            if (ImGui::Button("Retry...")) {
                m_app.reload_active_task();
            }
        },
    }(m_app.active_task_view());
}

namespace {

void setup_axis_links(ImAxis axis, double& v1, double& v2)
{
    if (v1 <= v2) {
        ImPlot::SetupAxisLinks(axis, &v1, &v2);
    } else {
        ImPlot::SetupAxisLinks(axis, &v2, &v1);
    }
}

bool is_mouse_inside_plot()
{
    if (!ImGui::IsMousePosValid()) {
        return false;
    }

    const ImVec2 bbmin = ImPlot::GetPlotPos();
    const ImVec2 bbmax = bbmin + ImPlot::GetPlotSize();
    const ImVec2 pos = ImGui::GetMousePos();
    return pos.x >= bbmin.x && pos.x <= bbmax.x && pos.y >= bbmin.y && pos.y <= bbmax.y;
}

void add_plot_marker(ImDrawList *draw_list, const ImVec2& pos)
{
    constexpr float half_width = 4;
    draw_list->AddRect(
        ImVec2(pos.x - half_width, pos.y - half_width),
        ImVec2(pos.x + half_width, pos.y + half_width),
        ImColor(128, 128, 128)
    );
}

constexpr float TextAutoalignMargin = 15;
constexpr float TextAutoalignPadding = 6;

/**
 * Add left-aligned text starting at (xp, yp), automatically right-aligning text if it would be
 * extend past xend
 */
void add_text_autoalign_left(
    ImDrawList *draw_list, const char *text, float xp, float yp, float xend
)
{
    const float text_width = ImGui::CalcTextSize(text).x;
    if (xp + text_width + TextAutoalignMargin > xend) {
        xp -= text_width + TextAutoalignPadding;
    } else {
        xp += TextAutoalignPadding;
    }
    draw_list->AddText(ImVec2(xp, yp), ImGui::GetColorU32(ImGuiCol_Text), text);
}

/**
 * Add right-aligned text ending at (xp, yp), automatically left-aligning text if it would extend
 * before xstart
 */
void add_text_autoalign_right(
    ImDrawList *draw_list, const char *text, float xp, float yp, float xstart
)
{
    const float text_width = ImGui::CalcTextSize(text).x;
    if (xp - text_width - TextAutoalignMargin < xstart) {
        xp += TextAutoalignPadding;
    } else {
        xp -= text_width + TextAutoalignPadding;
    }
    draw_list->AddText(ImVec2(xp, yp), ImGui::GetColorU32(ImGuiCol_Text), text);
}

void add_plot_vline(
    ImDrawList *draw_list,
    double xplot,
    double yplot,
    const ImVec2& pospx,
    fmt::format_string<double> xfmt,
    fmt::format_string<double> yfmt
)
{
    const ImVec2 plot_pos = ImPlot::GetPlotPos();
    const ImVec2 plot_size = ImPlot::GetPlotSize();
    const ImVec2 top(pospx.x, plot_pos.y);
    const ImVec2 bottom(pospx.x, top.y + plot_size.y);
    draw_list->AddLine(top, bottom, ImColor(128, 128, 128));

    const float xend = plot_pos.x + plot_size.x;
    add_text_autoalign_left(
        draw_list,
        fmt::vformat(xfmt, fmt::make_format_args(xplot)).c_str(),
        bottom.x,
        bottom.y - ImGui::GetTextLineHeightWithSpacing(),
        xend
    );
    add_text_autoalign_left(
        draw_list, fmt::vformat(yfmt, fmt::make_format_args(yplot)).c_str(), top.x, top.y, xend
    );
}

void draw_plot_cursor(double xplot, double yplot, fmt::format_string<double> yfmt)
{
    ImDrawList *draw_list = ImPlot::GetPlotDrawList();
    const auto pospx = ImPlot::PlotToPixels(xplot, yplot);
    add_plot_vline(draw_list, xplot, yplot, pospx, "t = {:g} s", yfmt);
    add_plot_marker(draw_list, pospx);
}

void draw_plot_hovered(const double *x, size_t n, const double *y, fmt::format_string<double> yfmt)
{
    const auto mouse = ImPlot::GetPlotMousePos();
    if (mouse.x > x[0]) {
        const double *const end = x + n;
        const double *xclosest = binary_search_closest(x, end, mouse.x);
        if (xclosest != end) {
            draw_plot_cursor(*xclosest, y[xclosest - x], yfmt);
        }
    }
}

void plot_line(const char *id, const std::vector<double>& x, const std::vector<double>& y)
{
    ImPlot::PlotLine(id, x.data(), y.data(), static_cast<int>(y.size()));
}

void plot_event(const char *id, const SwallowTaskData& data)
{
    ImPlot::PlotDigital(
        id, data.flow_time.data(), data.event.data(), static_cast<int>(data.event.size())
    );
}

void draw_plot_delta_selector(
    const char *id,
    widgets::PlotRangeSelector& selector,
    const ImColor& color,
    fmt::format_string<double> fmt_str = "t = {:g} s"
)
{
    (void) selector.update(id, 0, ImGuiMouseButton_Right);
    const widgets::PlotRange *range = selector.range();
    if (!range) {
        return;
    }

    widgets::draw_plot_range(*range, color);
    const ImVec2 plot_pos = ImPlot::GetPlotPos();
    const ImVec2 plot_size = ImPlot::GetPlotSize();
    const float yp = plot_pos.y + plot_size.y / 2;
    const double xrange = range->range();
    const std::string text = fmt::vformat(fmt_str, fmt::make_format_args(xrange));
    const double xmouse = ImPlot::GetPlotMousePos().x;
    const auto [xmin, xmax] = std::minmax(range->start, range->end);
    const double mid = (xmin + xmax) / 2;
    if (xmouse < mid) {
        add_text_autoalign_right(
            ImPlot::GetPlotDrawList(),
            text.c_str(),
            ImPlot::GetCurrentPlot()->XAxis(0).PlotToPixels(xmin),
            yp,
            plot_pos.x
        );
    } else {
        add_text_autoalign_left(
            ImPlot::GetPlotDrawList(),
            text.c_str(),
            ImPlot::GetCurrentPlot()->XAxis(0).PlotToPixels(xmax),
            yp,
            plot_pos.x + plot_size.x
        );
    };
}

// Add text in top left corner of plot
void add_plot_text(const char *str)
{
    ImPlot::PushPlotClipRect();
    ImPlot::GetPlotDrawList()->AddText(
        ImPlot::GetPlotPos() + ImGui::GetStyle().ItemSpacing,
        ImPlot::GetStyleColorU32(ImPlotCol_InlayText),
        str
    );
    ImPlot::PopPlotClipRect();
}

}; // namespace

void Gui::on_new_active_task(const app::App::ActiveTaskVariant& new_task)
{
    constexpr double EventBufferSecs = 6;

    VariantVisitor{
        [this](const app::ActiveSwallowLabellingTaskView& task) {
            const auto& info = task.info();
            const auto& time = task.data().flow_time;
            m_plot_summary_range = {
                std::max(info.event_range_secs.start - EventBufferSecs, time.front()),
                std::min(info.event_range_secs.end + EventBufferSecs, time.back()),
            };

            const auto *apnea_range = task.swallow_anpea_range();
            if (apnea_range) {
                m_apnea_temp_range = {apnea_range->start, apnea_range->end};
            } else {
                m_apnea_temp_range = {NAN, NAN};
            }
        },
        [this](const app::ActiveSwallowLabellingTaskErrorView&) {
            m_plot_summary_selector.reset();
            m_plot_summary_range = {NAN, NAN};
        },
    }(new_task);

    spdlog::debug(
        "Set new summary range to [{}, {}]", m_plot_summary_range.start, m_plot_summary_range.end
    );
}

Gui::Plotter::Plotter(
    std::string ylabel, fmt::format_string<double> cursor_format, widgets::PlotRange& xrange
) :
    m_ylabel(std::move(ylabel)), m_cursor_format(cursor_format), m_xrange(xrange)
{}

template <typename DrawFunc> void Gui::Plotter::draw(const char *id, float height, DrawFunc&& draw)
{
    constexpr ImPlotFlags Flags =
        ImPlotFlags_NoMouseText | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMenus;
    widgets::ScopedImID scoped_id(id);
    if (ImPlot::BeginPlot("##plot", {-1, height}, Flags)) {
        draw();
        ImPlot::EndPlot();
    }
}

void Gui::Plotter::plot_data(
    const std::vector<double>& x, const std::vector<double>& y, const SwallowTaskData& data
)
{
    constexpr ImU32 DeltaSelectorColor = IM_COL32(120, 120, 120, 50);

    setup_axis_links(ImAxis_X1, m_xrange.start, m_xrange.end);
    ImPlot::SetupAxis(
        ImAxis_Y1, m_ylabel.c_str(), ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit
    );
    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, x[0], x.back());

    plot_line("##line", x, y);
    if (is_mouse_inside_plot()) {
        draw_plot_hovered(x.data(), x.size(), y.data(), m_cursor_format);
    }
    plot_event("##event", data);
    draw_plot_delta_selector("##delta_selector", m_delta_selector, DeltaSelectorColor);
}

void Gui::draw_plots(app::ActiveSwallowLabellingTaskView& task_view)
{
    constexpr float SummaryPlotHeight = 75;
    constexpr unsigned int NumPlots = 2;
    const float plot_height = (ImGui::GetContentRegionAvail().y - SummaryPlotHeight) / NumPlots
        - ImGui::GetStyle().ItemSpacing.y;

    if (ImPlot::BeginAlignedPlots("##aligned_plots")) {
        m_flow_plotter.draw("##flow_plot", plot_height, [this, &task_view]() {
            draw_flow_plot(task_view);
        });
        m_audio_plotter.draw("##audio_plot", plot_height, [this, &task_view]() {
            draw_audio_plot(task_view);
        });
        ImPlot::EndAlignedPlots();
    }

    if (ImPlot::BeginPlot("##summary_plot", {-1, SummaryPlotHeight}, ImPlotFlags_CanvasOnly)) {
        constexpr ImPlotAxisFlags AxFlags = ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_AutoFit;
        ImPlot::SetupAxes(nullptr, nullptr, AxFlags, AxFlags);
        draw_plot_summary_selector();
        const auto& data = task_view.data();
        plot_line("##summary_flow_plot_line", data.flow_time, data.flow);
        plot_event("##summary_event", data);
        ImPlot::EndPlot();
    }
}

namespace {

void draw_plot_time_range(const models::TimeRange& range, const ImColor& color)
{
    widgets::draw_plot_range(range.start, range.end, color);
}

}; // namespace

void Gui::draw_flow_plot(app::ActiveSwallowLabellingTaskView& task_view)
{
    static constexpr ImColor ApneaSelectingColor = ImColor(1.0F, 1.0F, 0.0F, 0.1F);
    static constexpr ImColor ApneaSelectedColor = ImColor(1.0F, 1.0F, 0.0F, 0.4F);

    const auto& data = task_view.data();
    m_flow_plotter.plot_data(data.flow_time, data.flow, data);
    if (task_view.can_add_new_swallow_apnea_range()) {
        add_plot_text(HINT_ICON ICON_TEXT_SPACE
                      "Hold Ctrl and left click and drag to add apnea label");
    }

    if (task_view.can_add_new_swallow_apnea_range()) {
        const auto *selecting_range = m_apnea_range_selector.range();
        if (selecting_range) {
            widgets::draw_plot_range(*selecting_range, ApneaSelectingColor);
        }
        auto new_range = m_apnea_range_selector.update(
            "##apnea_range_selector", 0, ImGuiMouseButton_Left, ImGuiKey_LeftCtrl
        );
        if (new_range) {
            task_view.add_swallow_apnea_range(new_range->start, new_range->end);
        }
    }

    const auto *range = task_view.swallow_anpea_range();
    if (range) {
        if (task_view.can_edit_swallow_apnea_range()) {
            if (!m_apnea_range_dragger.is_editing()) {
                m_apnea_temp_range = {range->start, range->end};
            }
            widgets::draw_plot_range(m_apnea_temp_range, ApneaSelectedColor);
            if (m_apnea_range_dragger.update("##apnea_range_dragger", m_apnea_temp_range)) {
                task_view.set_swallow_apnea_range(m_apnea_temp_range.start, m_apnea_temp_range.end);
            }
        } else {
            draw_plot_time_range(*range, ApneaSelectedColor);
        }
    }
}

void Gui::draw_audio_plot(app::ActiveSwallowLabellingTaskView& task_view)
{
    const auto& data = task_view.data();
    m_audio_plotter.plot_data(data.audio_time, data.audio, data);
    if (task_view.can_add_new_ear_click_range()) {
        add_plot_text(HINT_ICON ICON_TEXT_SPACE
                      "Hold Ctrl and left click and drag to add ear click label(s)");
    }
}

void Gui::draw_plot_summary_selector()
{
    constexpr ImColor SummaryColor = {.5F, .5F, .5F, .6F};
    (void) m_plot_summary_selector.update("##plot_summary_selector");
    const widgets::PlotRange *new_range = m_plot_summary_selector.range();
    if (new_range) {
        m_plot_summary_range = *new_range;
    } else {
        (void) m_plot_summary_dragger.update("##plot_summary_dragger", m_plot_summary_range);
    }
    widgets::draw_plot_range(m_plot_summary_range, SummaryColor);
}

void Gui::set_scaling_factor(float scaling_factor)
{
    ImGui::GetStyle().ScaleAllSizes(scaling_factor);
}

void Gui::draw_menu_bar()
{
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Quit", "Alt+F4")) {
            stop();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        m_color_scheme_selector.draw();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools")) {
        if (ImGui::MenuItem("ImGui demo...") && !m_show_imgui_demo_window) {
            m_show_imgui_demo_window = true;
        }
        if (ImGui::MenuItem("Debug/metrics...") && !m_show_imgui_metrics) {
            spdlog::debug("Opening ImGui debug/metrics window");
            m_show_imgui_metrics = true;
        }
        if (ImGui::MenuItem("ImPlot demo...") && !m_show_implot_demo_window) {
            m_show_implot_demo_window = true;
        }
        ImGui::MenuItem("Show debug info", nullptr, &m_show_debug_info);
        ImGui::EndMenu();
    }
}

namespace {

const char *swallow_task_icon(const app::SwallowLabellingTask& task)
{
    if (task.error_msg().has_value()) {
        return FILE_ERR_ICON ICON_TEXT_SPACE;
    }

    if (task.is_annotated()) {
        return ANNOTATED_TASK_ICON ICON_TEXT_SPACE;
    }

    return "";
}

std::string task_info_str(const app::SwallowLabellingTask& task)
{
    const models::SwallowTaskInfo& info = task.info();
    return fmt::format(
        "{}Subject #{}, {}\nRepeat #{}, swallow #{}",
        swallow_task_icon(task),
        info.subject,
        swallow_test_type_string(info.test_type),
        info.repeatnum,
        info.swallownum
    );
}

}; // namespace

void Gui::draw_task_list()
{
    m_task_list_text_filter.Draw("##task_info_list_filter");

    // TODO: currently these buttons will advance to next/prev regardless of annotated state and
    // whether tasks are shown (due to filter). Need to lift task list state out of Gui.
    ImGui::SameLine();
    if (ImGui::ArrowButton("##prev_task", ImGuiDir_Left)) {
        m_app.decrement_active_task_index();
    }
    ImGui::SameLine();
    if (ImGui::ArrowButton("##next_task", ImGuiDir_Right)) {
        m_app.increment_active_task_index();
    }

    const auto& tasks = m_app.tasks();
    const std::size_t num_annotated = m_app.num_annotated_tasks();
    ImGui::Text(
        ANNOTATED_TASK_ICON ICON_TEXT_SPACE "Annotated: %zu task%s out of %zu",
        num_annotated,
        num_annotated == 1 ? "" : "s",
        tasks.size()
    );

    ImGui::BeginDisabled(num_annotated == 0);
    if (ImGui::Button(
            num_annotated != 0 && m_only_show_annotated_tasks ? "Show all" : "Show annotated only"
        ))
    {
        m_only_show_annotated_tasks = !m_only_show_annotated_tasks;
    }
    ImGui::EndDisabled();

    const std::size_t active_index = m_app.active_task_index();
    if (ImGui::BeginListBox("##task_info_list", {-1, -1})) {
        for (std::size_t i = 0; i < tasks.size(); i++) {
            const auto& task = tasks[i];
            const std::string str = task_info_str(task);
            if (m_task_list_text_filter.PassFilter(str.c_str())) {
                if (ImGui::Selectable(str.c_str(), active_index == i)) {
                    m_app.set_active_task_index(i);
                }
                const auto& err = task.error_msg();
                if (err.has_value() && ImGui::BeginItemTooltip()) {
                    ImGui::Text(ERR_ICON ICON_TEXT_SPACE "%s", err->c_str());
                    ImGui::EndTooltip();
                }
            }
        }
        ImGui::EndListBox();
    }
}

void Gui::draw_debug_info()
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text(
        DEBUG_INFO_ICON ICON_TEXT_SPACE
        "Mouse Position: [%.0f,%.0f]. Application average: %.3f ms/frame (%.1f FPS).",
        io.MousePos.x,
        io.MousePos.y,
        1000.0f / io.Framerate,
        io.Framerate
    );
}

bool shortcut_toggle(ImGuiKeyChord chord, bool& val)
{
    if (ImGui::Shortcut(chord)) {
        val = !val;
        return true;
    }
    return false;
}

namespace {

void draw_swallow_apnea_annotation_selection(app::ActiveSwallowLabellingTaskView& task_view)
{
    using enum app::SwallowApneaAnnotationStatus;

    widgets::ScopedImID id_scope("##apnea_annotation_status");

    auto status = task_view.swallow_apnea_annotation_status();
    bool is_ambiguous = task_view.swallow_is_ambiguous();
    bool status_changed = false;

    static std::array<widgets::RadioButtonField<app::SwallowApneaAnnotationStatus>, 4> src_options =
        {{
            {"ex-ex [1]", ExEx, ImGuiKey_1},
            {"ex-in [2]", ExIn, ImGuiKey_2},
            {"in-ex [3]", InEx, ImGuiKey_3},
            {"in-in [4]", InIn, ImGuiKey_4},
        }};
    for (const auto& opt : src_options) {
        bool selected = opt.value == status;
        if (ImGui::RadioButton(opt.label, selected) || ImGui::Shortcut(opt.key)) {
            if (!selected) {
                status = opt.value;
                status_changed = true;
                selected = true;
                spdlog::debug("Apnea SRC selection changed to {}", opt.label);
            }
        }
        if (selected) {
            ImGui::SameLine();
            if (ImGui::Checkbox("Ambiguous [a]", &is_ambiguous)
                || shortcut_toggle(ImGuiKey_A, is_ambiguous)) {
                spdlog::debug("Swallow apnea is_ambiguous changed: {}", is_ambiguous);
                task_view.set_swallow_is_ambiguous(is_ambiguous);
            }
        }
    }

    ImGui::Spacing();
    static std::array<widgets::RadioButtonField<app::SwallowApneaAnnotationStatus>, 3>
        other_options = {{
            {"No swallow", NoSwallow},
            {"Apnea cut-off", ApneaCutoff},
            {"FlowError", FlowError},
        }};
    if (widgets::radio_button_enums("##other_options", status, other_options)) {
        status_changed = true;
    }

    if (status_changed) {
        task_view.set_swallow_apnea_annotation_status(status);
    }
}

bool ear_click_annotation_status_radio(app::EarClickAnnotationStatus& status)
{
    using enum app::EarClickAnnotationStatus;
    static std::array<widgets::RadioButtonField<app::EarClickAnnotationStatus>, 3> options = {{
        {"Ok [e]", Ok, ImGuiKey_E},
        {"No ear click [w]", NoEarClick, ImGuiKey_W},
        {"Audio error", AudioError},
    }};
    return widgets::radio_button_enums("##earclick_annotation_status", status, options);
}

}; // namespace

void Gui::draw_label_editor(app::ActiveSwallowLabellingTaskView& task_view)
{
    ImGui::SeparatorText("Instructions");
    ImGui::TextWrapped("Single apnoea label required, may have multiple ear audio labels.");
    ImGui::TextWrapped("Code pattern using general breathing cycle (i.e. ignoring SNIF/SNRF");
    ImGui::TextWrapped("Expiratory flow is positive");

    ImGui::SeparatorText("Swallow apnea");
    if (const auto& error = task_view.swallow_apnea_label_error()) {
        ImGui::TextUnformatted(fmt::format(ERR_ICON ICON_TEXT_SPACE "{}", *error).c_str());
    }
    draw_swallow_apnea_annotation_selection(task_view);

    ImGui::SeparatorText("Ear clicks");
    if (const auto& error = task_view.earclick_label_error()) {
        ImGui::TextUnformatted(fmt::format(ERR_ICON ICON_TEXT_SPACE "{}", *error).c_str());
    }
    app::EarClickAnnotationStatus ear_click_status = task_view.ear_click_annotation_status();
    if (ear_click_annotation_status_radio(ear_click_status)) {
        task_view.set_ear_click_annotation_status(ear_click_status);
    }
}

}; // namespace recap::labeller::gui
