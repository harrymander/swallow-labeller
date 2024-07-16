#define IMGUI_DEFINE_MATH_OPERATORS

#include "gui.hpp"

#include "../util.hpp"
#include "../variant-visitor.hpp"
#include "font.hpp"
#include "widgets/plot-range-selector.hpp"
#include "widgets/plot-range.hpp"

#include <IconsFontAwesome6.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <optional>

#define FILE_ERR_ICON ICON_FA_FILE_CIRCLE_EXCLAMATION
#define ERR_ICON ICON_FA_TRIANGLE_EXCLAMATION
#define DEBUG_INFO_ICON ICON_FA_GEAR
#define ANNOTATED_TASK_ICON ICON_FA_SQUARE_CHECK
#define ICON_TEXT_SPACE "  "

namespace recap::labeller::gui {

namespace {

std::optional<std::string> get_custom_ini_path()
{
    const char *const env = std::getenv("RECAP_LABELLER_IMGUI_INI_PATH");
    if (env) {
        std::filesystem::path path(env);
        if (std::filesystem::is_directory(path)) {
            spdlog::error("Invalid ImGui INI path: '{}' is a directory", path);
        } else if (!std::filesystem::is_directory(path.parent_path())) {
            spdlog::error(
                "Invalid ImGui INI path: parent directory '{}' does not exist or not a directory",
                path.parent_path()
            );
        } else {
            std::string path_str = path.make_preferred().string();
            spdlog::info("Custom ImGui INI path: {}", path_str);
            return path_str;
        }

        spdlog::warn("Ignoring RECAP_LABELLER_IMGUI_INI_PATH, using default ImGui INI path");
    } else {
        spdlog::debug("RECAP_LABELLER_IMGUI_INI_PATH not set, using default ImGui INI path");
    }

    return std::nullopt;
}

}; // namespace

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
    auto custom_ini_path = get_custom_ini_path();
    if (custom_ini_path) {
        m_ini_path = std::move(*custom_ini_path);
        io.IniFilename = m_ini_path->c_str();
    }

    setup_fonts();

    on_new_active_task(m_app.active_task_view());
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

void Gui::draw()
{
    constexpr ImGuiWindowFlags WindowFlags =
        (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove
         | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

    if (ImGui::BeginMainMenuBar()) {
        draw_menu_bar();
        ImGui::EndMainMenuBar();
    }
    setup_dockspace();

    if (ImGui::Begin(TaskListWindowId, nullptr, WindowFlags)) {
        draw_task_list();
        ImGui::End();
    }

    if (ImGui::Begin(MainWindowId, nullptr, WindowFlags)) {
        draw_main_window();
        ImGui::End();
    }

    if (ImGui::Begin(LabelInfoWindowId, nullptr, WindowFlags)) {
        ImGui::End();
    }

    if (m_show_imgui_demo_window) {
        ImGui::ShowDemoWindow(&m_show_imgui_demo_window);
    }
    if (m_show_imgui_metrics) {
        ImGui::ShowMetricsWindow(&m_show_imgui_metrics);
    }
    if (m_show_implot_demo_window) {
        ImPlot::ShowDemoWindow(&m_show_implot_demo_window);
    }

    m_first_draw = false;
}

void Gui::setup_dockspace() const
{
    constexpr ImGuiDockNodeFlags DockspaceFlags =
        (ImGuiDockNodeFlags_NoUndocking | ImGuiDockNodeFlags_AutoHideTabBar
         | ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoTabBar);

    constexpr float SidebarRatio = 0.2;
    constexpr float LabelInfoRatioX = 0.2 / (1 - SidebarRatio);

    // If the dockspace ID already exists, the the node sizes are already set in imgui.ini
    ImGuiID id = ImGui::GetID("##dockspace");
    if (m_first_draw && ImGui::DockBuilderGetNode(id) == nullptr) [[unlikely]] {
        ImGui::DockSpaceOverViewport(id, ImGui::GetMainViewport(), DockspaceFlags);
        // The following is adapted from
        // https://gist.github.com/AidanSun05/953f1048ffe5699800d2c92b88c36d9f
        spdlog::debug("Setting up dockspace");
        ImGui::DockBuilderRemoveNode(id);
        ImGui::DockBuilderAddNode(id);

        ImGuiID dock_tasklist;
        ImGuiID dock_main;
        ImGuiID dock_label_info;
        ImGui::DockBuilderSplitNode(id, ImGuiDir_Left, SidebarRatio, &dock_tasklist, &dock_main);
        ImGui::DockBuilderSplitNode(
            dock_main, ImGuiDir_Right, LabelInfoRatioX, &dock_label_info, &dock_main
        );

        ImGui::DockBuilderDockWindow(TaskListWindowId, dock_tasklist);
        ImGui::DockBuilderDockWindow(MainWindowId, dock_main);
        ImGui::DockBuilderDockWindow(LabelInfoWindowId, dock_label_info);
        ImGui::DockBuilderFinish(id);
    } else {
        ImGui::DockSpaceOverViewport(id, ImGui::GetMainViewport(), DockspaceFlags);
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
        [this](const app::ActiveSwallowLabellingTaskView& task) { draw_plots(task.data()); },
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
        const double *xclosest = util::binary_search_closest(x, end, mouse.x);
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

}; // namespace

void Gui::on_new_active_task(const app::App::ActiveTaskVariant& new_task)
{
    constexpr double EventBufferSecs = 6;

    m_plot_summary_range = VariantVisitor{
        [](const app::ActiveSwallowLabellingTaskView& task) -> widgets::PlotRange {
            const auto& info = task.info();
            const auto& time = task.data().flow_time;
            return widgets::PlotRange{
                std::max(info.event_range_secs.start - EventBufferSecs, time.front()),
                std::min(info.event_range_secs.end + EventBufferSecs, time.back()),
            };
        },
        [this](const app::ActiveSwallowLabellingTaskErrorView&) -> widgets::PlotRange {
            m_plot_summary_selector.reset();
            return widgets::PlotRange{NAN, NAN};
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

bool Gui::Plotter::begin(const char *id)
{
    constexpr ImPlotFlags Flags =
        ImPlotFlags_NoMouseText | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMenus;
    ImGui::PushID(id);
    if (ImPlot::BeginPlot("##plot", {-1, 0}, Flags)) {
        return true;
    }
    ImGui::PopID();
    return false;
}

// Only call if begin returns true!
void Gui::Plotter::end()
{
    ImPlot::EndPlot();
    ImGui::PopID();
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

void Gui::draw_plots(const SwallowTaskData& data)
{
    constexpr float SummaryPlotHeight = 75;

    if (ImPlot::BeginAlignedPlots("##aligned_plots")) {
        if (m_flow_plotter.begin("##flow_plot")) {
            m_flow_plotter.plot_data(data.flow_time, data.flow, data);
            m_flow_plotter.end();
        }
        if (m_audio_plotter.begin("##audio_plot")) {
            m_audio_plotter.plot_data(data.audio_time, data.audio, data);
            m_audio_plotter.end();
        }

        ImPlot::EndAlignedPlots();
    }

    if (ImPlot::BeginPlot("##summary_plot", {-1, SummaryPlotHeight}, ImPlotFlags_CanvasOnly)) {
        constexpr ImPlotAxisFlags AxFlags = ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_AutoFit;
        ImPlot::SetupAxes(nullptr, nullptr, AxFlags, AxFlags);
        draw_plot_summary_selector();
        plot_line("##summary_flow_plot_line", data.flow_time, data.flow);
        plot_event("##summary_event", data);
        ImPlot::EndPlot();
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
    const SwallowTaskInfo& info = task.info();
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
    const auto& tasks = m_app.tasks();
    const std::size_t num_annotated = m_app.num_annotated_tasks();
    ImGui::Text(
        ANNOTATED_TASK_ICON ICON_TEXT_SPACE "%zu task%s out of %zu annotated",
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

    m_task_list_text_filter.Draw("Filter##task_info_list_filter");
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
}; // namespace recap::labeller::gui
