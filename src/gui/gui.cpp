#define IMGUI_DEFINE_MATH_OPERATORS

#include "gui.hpp"

#include "app/app.hpp"
#include "gui/font.hpp"
#include "gui/widgets/plot-range-selector.hpp"
#include "gui/widgets/plot-range.hpp"
#include "gui/widgets/radio-button-enum.hpp"
#include "gui/widgets/util.hpp"
#include "models/time-range.hpp"
#include "util/optutil.hpp"
#include "util/os.hpp"
#include "util/util.hpp"
#include "util/variant-visitor.hpp"

#include <IconsFontAwesome6.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <implot.h>
#include <magic_enum.hpp>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <variant>

#define FILE_ERR_ICON ICON_FA_FILE_CIRCLE_EXCLAMATION
#define ERR_ICON ICON_FA_TRIANGLE_EXCLAMATION
#define DEBUG_INFO_ICON ICON_FA_GEAR
#define ANNOTATED_TASK_ICON ICON_FA_SQUARE_CHECK
#define HINT_ICON ICON_FA_LIGHTBULB
#define DELETE_ICON ICON_FA_TRASH_CAN
#define EXIT_ICON ICON_FA_XMARK
#define SHUFFLE_ICON ICON_FA_SHUFFLE
#define UNSHUFFLE_ICON ICON_FA_SORT
#define ICON_TEXT_SPACE "  "
constexpr float LabelSummaryHeight = 8; // Same as default ImPlotStyle::DigitalBitHeight

namespace recap::labeller::gui {

namespace {

namespace {

struct GuiColors {
    // Generated using
    // http://www.workwithcolor.com/hsl-color-schemer-01.htm?cp=FC655A&ch=4-96-67&cm=0&sm=4&mil=0&dst=60

    using RGB = std::tuple<uint8_t, uint8_t, uint8_t>;

    static constexpr std::array ApneaLabelColors = {
        RGB(0xF1, 0xFC, 0x5A),
        RGB(0x5A, 0xFC, 0x65),
        RGB(0x5A, 0xF1, 0xFC),
        RGB(0x65, 0x5A, 0xFC),
    };

    static constexpr RGB EarClickLabelColor = {0xFC, 0x5A, 0xF1};
    static constexpr RGB EventLabelColor = {0xFC, 0x65, 0x5A};

    static constexpr ImU32
    apnea_label_color(app::SwallowApneaAnnotationStatus status, uint8_t alpha = 0xff)
    {
        using enum app::SwallowApneaAnnotationStatus;
        return color(ApneaLabelColors[magic_enum::enum_integer(status)], alpha);
    }

    static constexpr ImU32 color(const RGB& rgb, uint8_t alpha = 0xff)
    {
        return IM_COL32(std::get<0>(rgb), std::get<1>(rgb), std::get<2>(rgb), alpha);
    }
};

}; // namespace

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
    const auto env = os::getenv("RECAP_LABELLER_IMGUI_INI_PATH");
    if (env.has_value()) {
        if (!env->empty()) {
            std::filesystem::path path(*env);
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
    m_app.stop();
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

    const auto& critical_error = m_app.critical_error();
    if (critical_error.has_value()) {
        draw_critical_error(*critical_error);
    }

    if (m_app.unsaved_task_switch_blocked()) {
        draw_unsaved_task_prompt();
    }

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

void Gui::draw_critical_error(const std::string& error)
{
    constexpr ImU32 TitleColor = 0xCC2929FF;
    constexpr ImVec2 CentrePos = {0.5F, 0.5F};
    widgets::ScopedImColor color_scope(ImGuiCol_TitleBgActive, TitleColor);

    static const char *modal_title = ERR_ICON ICON_TEXT_SPACE "Critical error##crit_err_modal";

    if (!m_critical_error_modal_open) {
        m_critical_error_modal_open = true;
        ImGui::OpenPopup(modal_title);
        ImGui::SetNextWindowPos(
            ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, CentrePos
        );
    }

    if (ImGui::BeginPopupModal(modal_title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s\n(Please email Harry!)", error.c_str());
        if (widgets::ButtonRed(EXIT_ICON ICON_TEXT_SPACE "Quit")) {
            stop();
        }
        ImGui::EndPopup();
    }
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
    constexpr float MinRatio = 0.1F;
    constexpr float MaxRatio = 0.25F;

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

    const auto& task = m_app.tasks().at(m_app.active_task_index());
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

void plot_event(const models::TimeRange& range)
{
    constexpr ImU32 color = GuiColors::color(GuiColors::EventLabelColor);
    widgets::draw_plot_range(range.start, range.end, color, -LabelSummaryHeight);
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

Gui::Annotator::Annotator(const app::ActiveSwallowLabellingTaskView& task_view) :
    apnea_temp_range(optutil::map_or(
        task_view.swallow_anpea_range(),
        [](const models::TimeRange& range) {
            return widgets::PlotRange{range.start, range.end};
        },
        widgets::PlotRange{NAN, NAN}
    )),
    note(task_view.note())
{}

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
            spdlog::debug(
                "Set new summary range to [{}, {}]",
                m_plot_summary_range.start,
                m_plot_summary_range.end
            );

            m_annotator = Annotator(task);
        },
        [this](const app::ActiveSwallowLabellingTaskErrorView&) {
            m_plot_summary_selector.reset();
            m_plot_summary_range = {NAN, NAN};
            m_annotator = Annotator();
        },
    }(new_task);
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
    const std::vector<double>& x, const std::vector<double>& y, const models::TimeRange& event_range
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
    plot_event(event_range);
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
        plot_event(task_view.info().event_range_secs);
        ImPlot::EndPlot();
    }
}

void Gui::draw_flow_plot(app::ActiveSwallowLabellingTaskView& task_view)
{
    const auto& data = task_view.data();
    m_flow_plotter.plot_data(data.flow_time, data.flow, task_view.info().event_range_secs);

    draw_apnea_label_region(task_view);
    draw_earclick_label_regions(task_view, LabelSummaryHeight, true);

    if (task_view.can_add_new_swallow_apnea_range()) {
        add_plot_text(HINT_ICON ICON_TEXT_SPACE
                      "Hold Ctrl and left click and drag to add apnea label");
    }

    if (task_view.can_add_new_swallow_apnea_range()) {
        auto new_range = m_annotator.apnea_range_selector.update(
            "##apnea_range_selector", 0, ImGuiMouseButton_Left, ImGuiKey_LeftCtrl
        );
        if (new_range) {
            task_view.add_swallow_apnea_range(new_range->start, new_range->end);
        }
    }

    const auto *range = task_view.swallow_anpea_range();
    if (range && task_view.can_edit_swallow_apnea_range()) {
        if (!m_annotator.apnea_range_dragger.is_editing()) {
            m_annotator.apnea_temp_range = {range->start, range->end};
        }
        if (m_annotator.apnea_range_dragger.update(
                "##apnea_range_dragger", m_annotator.apnea_temp_range
            )) {
            task_view.set_swallow_apnea_range(
                m_annotator.apnea_temp_range.start, m_annotator.apnea_temp_range.end
            );
        }
    }
}

void Gui::draw_apnea_label_region(
    const app::ActiveSwallowLabellingTaskView& task_view, float height, bool selected_color
) const
{
    constexpr uint8_t SelectingAlpha = 0x33;
    constexpr uint8_t SelectedAlpha = 0x66;

    if (!(task_view.can_add_new_ear_click_range() || task_view.can_edit_swallow_apnea_range())) {
        return;
    }

    const auto *selecting_range = m_annotator.apnea_range_selector.range();
    const app::SwallowApneaAnnotationStatus status = task_view.swallow_apnea_annotation_status();
    if (selecting_range) {
        widgets::draw_plot_range(
            *selecting_range,
            GuiColors::apnea_label_color(status, selected_color ? SelectedAlpha : SelectingAlpha),
            height
        );
    } else {
        const auto *range = task_view.swallow_anpea_range();
        if (range) {
            const ImU32 color = GuiColors::apnea_label_color(status, SelectedAlpha);
            if (m_annotator.apnea_range_dragger.is_editing()) {
                widgets::draw_plot_range(m_annotator.apnea_temp_range, color, height);
            } else {
                widgets::draw_plot_range(range->start, range->end, color, height);
            }
        }
    }
}

void Gui::draw_audio_plot(app::ActiveSwallowLabellingTaskView& task_view)
{
    const auto& data = task_view.data();
    m_audio_plotter.plot_data(data.audio_time, data.audio, task_view.info().event_range_secs);

    draw_earclick_label_regions(task_view);
    draw_apnea_label_region(task_view, LabelSummaryHeight, true);

    if (task_view.can_add_new_ear_click_range()) {
        add_plot_text(HINT_ICON ICON_TEXT_SPACE
                      "Hold Ctrl and left click and drag to add ear click label(s)");
        auto new_range = m_annotator.earclick_range_selector.update(
            "##earclick_new_range_selector", 0, ImGuiMouseButton_Left, ImGuiKey_LeftCtrl
        );
        if (new_range) {
            auto new_id = task_view.add_ear_click_label(new_range->start, new_range->end);
            if (new_id) {
                m_annotator.selected_ear_click_id = new_id;
            }
        }
    }

    if (task_view.can_add_new_ear_click_range() && m_annotator.selected_ear_click_id.has_value()) {
        const auto *range = task_view.ear_click_label(*m_annotator.selected_ear_click_id);
        if (range) {
            if (!m_annotator.earclick_range_dragger.is_editing()) {
                m_annotator.earclick_temp_range = {range->range.start, range->range.end};
            }
            if (m_annotator.earclick_range_dragger.update(
                    "##earclick_range_dragger", m_annotator.earclick_temp_range
                ))
            {
                task_view.set_ear_click_label(
                    *m_annotator.selected_ear_click_id,
                    m_annotator.earclick_temp_range.start,
                    m_annotator.earclick_temp_range.end
                );
            }
        } else {
            spdlog::error("No range for ID = {}", *m_annotator.selected_ear_click_id);
            m_annotator.selected_ear_click_id.reset();
        }
    }
}

void Gui::draw_earclick_label_regions(
    const app::ActiveSwallowLabellingTaskView& task_view, float height, bool selected_color
) const
{
    constexpr ImColor Color = GuiColors::color(GuiColors::EarClickLabelColor, 0x33);
    constexpr ImColor ColorHovered = GuiColors::color(GuiColors::EarClickLabelColor, 0x44);
    constexpr ImColor ColorSelected = GuiColors::color(GuiColors::EarClickLabelColor, 0x66);

    const auto *labels = task_view.ear_click_labels();
    if (labels == nullptr) {
        return;
    }

    for (const auto& label : *labels) {
        if (m_annotator.earclick_range_dragger.is_editing()
            && m_annotator.selected_ear_click_id == label.id) [[unlikely]]
        {
            widgets::draw_plot_range(m_annotator.earclick_temp_range, ColorSelected, height);
        } else {
            widgets::draw_plot_range(
                label.range.start,
                label.range.end,
                selected_color || m_annotator.selected_ear_click_id == label.id ?
                    ColorSelected :
                    (m_annotator.hovered_ear_click_id == label.id ? ColorHovered : Color),
                height
            );
        }
    }

    const auto *selector_range = m_annotator.earclick_range_selector.range();
    if (selector_range) {
        widgets::draw_plot_range(*selector_range, selected_color ? ColorSelected : Color, height);
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

    const auto *task_view =
        std::get_if<app::ActiveSwallowLabellingTaskView>(&m_app.active_task_view());
    if (task_view) {
        draw_apnea_label_region(*task_view, LabelSummaryHeight, true);
        draw_earclick_label_regions(*task_view, LabelSummaryHeight, true);
    }
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

const char *swallow_task_icon(const app::SwallowLabellingTask& task, bool is_annotated)
{
    if (task.error_msg().has_value()) {
        return FILE_ERR_ICON ICON_TEXT_SPACE;
    }

    if (is_annotated) {
        return ANNOTATED_TASK_ICON ICON_TEXT_SPACE;
    }

    return "";
}

std::string task_info_str(const app::SwallowLabellingTask& task, bool is_annotated)
{
    const models::SwallowTaskInfo& info = task.info();
    return fmt::format(
        "{}Subject #{}, {}\nRepeat #{}, swallow #{}",
        swallow_task_icon(task, is_annotated),
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

    const auto& tasks = m_app.tasks();

    const ImVec2 num_annotated_pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(
        {num_annotated_pos.x, num_annotated_pos.y + ImGui::GetTextLineHeightWithSpacing()}
    );

    const float button_padding = 2 * ImGui::GetStyle().ItemSpacing.x;

    const char *shuffle_button_str = SHUFFLE_ICON ICON_TEXT_SPACE "Shuffle";
    const float shuffle_button_width = button_padding + ImGui::CalcTextSize(shuffle_button_str).x;
    if (m_app.tasks_shuffled()) {
        if (ImGui::Button(UNSHUFFLE_ICON ICON_TEXT_SPACE "Sort", {shuffle_button_width, 0})) {
            m_app.unshuffle_tasks();
        }
    } else {
        if (ImGui::Button(shuffle_button_str, {shuffle_button_width, 0})) {
            m_app.shuffle_tasks();
        }
    }

    ImGui::SameLine();
    bool scroll_to_selected_task = false;
    const char *show_annotated_str = "Show annotated only";
    const float show_annotated_button_width =
        button_padding + ImGui::CalcTextSize(show_annotated_str).x;
    if (ImGui::Button(
            m_only_show_annotated_tasks ? "Show all" : show_annotated_str,
            {show_annotated_button_width, 0}
        ))
    {
        m_only_show_annotated_tasks = !m_only_show_annotated_tasks;
        scroll_to_selected_task = true;
    }

    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32_BLACK_TRANS);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32_BLACK_TRANS);
    ImGui::BeginGroup();
    const bool scroll_to_top = ImGui::SmallButton(ICON_FA_ARROWS_UP_TO_LINE);
    ImGui::SetItemTooltip("Scroll to top");
    if (ImGui::SmallButton(ICON_FA_ARROWS_TO_DOT)) {
        scroll_to_selected_task = true;
    }
    ImGui::SetItemTooltip("Scroll to active task");
    const bool scroll_to_bottom = ImGui::SmallButton(ICON_FA_ARROWS_DOWN_TO_LINE);
    ImGui::SetItemTooltip("Scroll to bottom");
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::PopStyleColor(3);

    const std::size_t active_index = m_app.active_task_index();
    if (!scroll_to_selected_task) {
        scroll_to_selected_task = m_task_list_last_active_index != active_index;
    }
    m_task_list_last_active_index = active_index;
    std::size_t num_annotated = 0;

    std::size_t new_active_index = active_index;
    if (ImGui::BeginListBox("##task_info_list", {-1, -1})) {
        if (scroll_to_top) {
            ImGui::SetScrollHereY();
        }

        std::size_t i = 0;
        for (const auto& task : tasks.items()) {
            const bool has_annotation = m_app.task_has_annotation(task);
            if (has_annotation) {
                num_annotated += 1;
            }
            if (!m_only_show_annotated_tasks || has_annotation) {
                const std::string str = task_info_str(task, has_annotation);
                if (m_task_list_text_filter.PassFilter(str.c_str())) {
                    const bool selected = active_index == i;
                    if (ImGui::Selectable(str.c_str(), selected)) {
                        new_active_index = i;
                    }
                    if (selected && scroll_to_selected_task && !ImGui::IsItemVisible()) {
                        ImGui::ScrollToItem();
                    }
                    const auto& err = task.error_msg();
                    if (err.has_value() && ImGui::BeginItemTooltip()) {
                        ImGui::Text(ERR_ICON ICON_TEXT_SPACE "%s", err->c_str());
                        ImGui::EndTooltip();
                    }
                }
            }

            i += 1;
        }

        if (scroll_to_bottom) {
            ImGui::SetScrollHereY();
        }
        ImGui::EndListBox();
    }

    ImGui::SetCursorPos(num_annotated_pos);
    ImGui::Text(
        ANNOTATED_TASK_ICON ICON_TEXT_SPACE "Annotated: %zu task%s out of %zu",
        num_annotated,
        num_annotated == 1 ? "" : "s",
        tasks.size()
    );

    if (active_index != new_active_index) {
        m_app.set_active_task_index(new_active_index);
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

namespace {

bool shortcut_toggle(ImGuiKeyChord chord, bool& val)
{
    if (widgets::global_shortcut(chord)) {
        val = !val;
        return true;
    }
    return false;
}

namespace {

bool colored_radio_button(const char *label, bool selected, const ImColor& base_color)
{
    float hue;
    float sat;
    float val;
    ImGui::ColorConvertRGBtoHSV(
        base_color.Value.x, base_color.Value.y, base_color.Value.z, hue, sat, val
    );
    const auto check_color = ImColor::HSV(hue, sat, val + 0.3F);
    const auto bg_color = ImColor::HSV(hue, sat, val - 0.4F);
    const auto hover_color = ImColor::HSV(hue, sat, val - 0.3F);
    const auto active_color = ImColor::HSV(hue, sat, val - 0.25F);
    widgets::ScopedImColor color_scope{
        {ImGuiCol_FrameBg, bg_color},
        {ImGuiCol_FrameBgHovered, hover_color},
        {ImGuiCol_FrameBgActive, active_color},
        {ImGuiCol_CheckMark, check_color},
    };

    return ImGui::RadioButton(label, selected);
}

}; // namespace

void draw_swallow_apnea_annotation_selection(app::ActiveSwallowLabellingTaskView& task_view)
{
    using enum app::SwallowApneaAnnotationStatus;

    widgets::ScopedImID id_scope("##apnea_annotation_status");

    auto status = task_view.swallow_apnea_annotation_status();
    bool is_ambiguous = task_view.swallow_is_ambiguous();
    bool status_changed = false;

    using Option = widgets::RadioButtonField<app::SwallowApneaAnnotationStatus>;
    constexpr std::array SrcOptions = {
        Option("ex-ex [1]", ExEx, ImGuiKey_1),
        Option("ex-in [2]", ExIn, ImGuiKey_2),
        Option("in-ex [3]", InEx, ImGuiKey_3),
        Option("in-in [4]", InIn, ImGuiKey_4),
    };
    for (const auto& opt : SrcOptions) {
        bool selected = opt.value == status;
        const bool radio_clicked =
            colored_radio_button(opt.label, selected, GuiColors::apnea_label_color(opt.value));
        if (radio_clicked || widgets::global_shortcut(opt.key)) {
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
    constexpr std::array OtherOptions = {
        Option("No swallow", NoSwallow),
        Option("Apnea cut-off", ApneaCutoff),
        Option("FlowError", FlowError),
    };
    if (widgets::radio_button_enums("##other_options", status, OtherOptions)) {
        status_changed = true;
    }

    if (status_changed) {
        task_view.set_swallow_apnea_annotation_status(status);
    }
}

bool ear_click_annotation_status_radio(app::EarClickAnnotationStatus& status)
{
    using enum app::EarClickAnnotationStatus;
    using Option = widgets::RadioButtonField<app::EarClickAnnotationStatus>;
    constexpr std::array Options = {
        Option("Ok [e]", Ok, ImGuiKey_E),
        Option("No ear click [w]", NoEarClick, ImGuiKey_W),
        Option("Audio error", AudioError),
    };
    return widgets::radio_button_enums("##earclick_annotation_status", status, Options);
}

bool delete_button()
{
    constexpr float ButtonCornerRadius = 5;
    widgets::ScopedImStyle style(ImGuiStyleVar_FrameRounding, ButtonCornerRadius);
    return widgets::ButtonRed(DELETE_ICON);
}

bool delete_button(const char *id)
{
    widgets::ScopedImID id_scope(id);
    return delete_button();
}

template <typename... Args> bool delete_label_button(Args&&...args)
{
    const bool clicked = delete_button(std::forward<Args>(args)...);
    ImGui::SetItemTooltip("Delete label");
    return clicked;
}

}; // namespace

bool Gui::draw_annotation_submit(app::ActiveSwallowLabellingTaskView& task_view)
{
    static const char *del_str = ICON_TEXT_SPACE ICON_FA_TRASH_CAN ICON_TEXT_SPACE;
    const float del_button_width =
        ImGui::CalcTextSize(del_str).x + ImGui::GetStyle().ItemInnerSpacing.x * 4;
    float button_height = del_button_width;

    const bool can_delete = task_view.can_delete_annotation();
    const float submit_button_width =
        can_delete ? ImGui::GetContentRegionAvail().x - del_button_width : -1;

    ImGui::BeginDisabled(!task_view.can_save_annotation());
    const bool save_task = ImGui::Button("Save [Ctrl+S]", {submit_button_width, button_height})
        || widgets::global_shortcut(ImGuiMod_Ctrl | ImGuiKey_S);
    ImGui::EndDisabled();

    if (can_delete) {
        ImGui::SameLine();
        if (widgets::ButtonRed(del_str, {del_button_width, button_height})) {
            task_view.delete_annotation();
            m_annotator = Annotator(task_view);
        }
        ImGui::SetItemTooltip("Delete annotation");
    }

    bool auto_advance = m_app.auto_advance_on_save();
    if (ImGui::Checkbox("Auto-advance to next task", &auto_advance)) {
        m_app.set_auto_advance_on_save(auto_advance);
        spdlog::debug("{}abled auto-advance on save", auto_advance ? "En" : "Dis");
    }

    return save_task;
}

void Gui::draw_label_editor(app::ActiveSwallowLabellingTaskView& task_view)
{
    constexpr float NoteHeightLines = 3;

    const bool to_save_annotation = draw_annotation_submit(task_view);

    ImGui::SeparatorText("Instructions");
    ImGui::TextWrapped("Single apnoea label required, may have multiple ear audio labels.");
    ImGui::TextWrapped("Code pattern using general breathing cycle (i.e. ignoring SNIF/SNRF");
    ImGui::TextWrapped("Expiratory flow is positive");

    ImGui::SeparatorText("Note");
    const float note_height =
        (NoteHeightLines - 1) * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetTextLineHeight();
    bool update_note =
        ImGui::InputTextMultiline("##annotation_note_input", &m_annotator.note, {-1, note_height});
    if (ImGui::SmallButton("Clear##clear_note_text")) {
        m_annotator.note.clear();
        update_note = true;
    }
    if (update_note) {
        task_view.set_note(m_annotator.note);
    }

    ImGui::SeparatorText("Swallow apnea");
    if (const auto& error = task_view.swallow_apnea_label_error()) {
        ImGui::TextUnformatted(fmt::format(ERR_ICON ICON_TEXT_SPACE "{}", *error).c_str());
    }
    const auto *apnea_range = task_view.swallow_anpea_range();
    if (apnea_range) {
        ImGui::Text(
            "Apnea: [%.3f, %.3f] s (Δ = %.3f s)",
            apnea_range->start,
            apnea_range->end,
            apnea_range->end - apnea_range->start
        );
        if (task_view.can_delete_swallow_apnea_range()) {
            ImGui::SameLine();
            if (delete_label_button("##delete_swallow_apnea_range")) {
                task_view.delete_swallow_apnea_range();
            }
        }
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
    const auto *labels = task_view.ear_click_labels();
    if (task_view.can_add_new_ear_click_range() && labels) {
        if (labels->empty()) {
            ImGui::TextWrapped(HINT_ICON ICON_TEXT_SPACE
                               "No ear click labels - hold Ctrl and left click on audio plot to "
                               "add one, or select the relevant option above");
        } else {
            widgets::ScopedImID scoped_id("##earclick_label_list");
            if (ImGui::BeginListBox("##listbox", {-1, -1})) {
                draw_earclick_labels_listbox(task_view, *labels);
                ImGui::EndListBox();
            }
        }
    }

    if (to_save_annotation) {
        m_app.save_active_task();
    }
}

void Gui::draw_earclick_labels_listbox(
    app::ActiveSwallowLabellingTaskView& task_view, const std::vector<app::EarClickLabel>& labels
)
{
    static const char *remove_button_str = DELETE_ICON;
    constexpr ImVec2 SelectableTextAlign = {0, 0.5};
    widgets::ScopedImStyle selectable_style(ImGuiStyleVar_SelectableTextAlign, SelectableTextAlign);

    const float label_height = ImGui::GetTextLineHeightWithSpacing();
    const float label_width = ImGui::GetContentRegionAvail().x
        - (ImGui::CalcTextSize(remove_button_str).x + 2 * ImGui::GetStyle().ItemSpacing.x);

    std::optional<app::EarClickLabel::ID> id_to_remove = std::nullopt;
    m_annotator.hovered_ear_click_id.reset();
    for (const auto& label : labels) {
        widgets::ScopedImID label_id_scope(static_cast<int>(label.id));
        std::string str = fmt::format(
            "Ear click {} [{:.3f}, {:.3f} s] (Δ = {:.3f} s)",
            label.id,
            label.range.start,
            label.range.end,
            label.range.end - label.range.start
        );
        const bool selected =
            optutil::has_value_and_equal(m_annotator.selected_ear_click_id, label.id);
        if (ImGui::Selectable(str.c_str(), selected, 0, {label_width, label_height})) {
            if (selected) {
                spdlog::debug("De-selecting ear click label ID={}", label.id);
                m_annotator.selected_ear_click_id.reset();
            } else {
                spdlog::debug("Selecting ear click label ID={}", label.id);
                m_annotator.selected_ear_click_id = label.id;
            }
        }
        if (ImGui::IsItemHovered()) {
            m_annotator.hovered_ear_click_id = label.id;
        }

        ImGui::SameLine();
        if (delete_label_button()) {
            id_to_remove = label.id;
        }
        if (ImGui::IsItemHovered()) {
            m_annotator.hovered_ear_click_id = label.id;
        }
    }

    if (id_to_remove.has_value()) {
        task_view.remove_ear_click_label(*id_to_remove);
        if (id_to_remove == m_annotator.selected_ear_click_id) {
            m_annotator.selected_ear_click_id.reset();
        }
        if (id_to_remove == m_annotator.hovered_ear_click_id) {
            m_annotator.hovered_ear_click_id.reset();
        }
    }
}

void Gui::draw_unsaved_task_prompt()
{
    static const char *ModalId = ERR_ICON ICON_TEXT_SPACE "Unsaved annotation";
    constexpr ImVec2 CentrePos = {0.5F, 0.5F};
    constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;

    if (!m_unsaved_task_switch_modal_open) {
        m_unsaved_task_switch_modal_open = true;
        ImGui::OpenPopup(ModalId);
        ImGui::SetNextWindowPos(
            ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, CentrePos
        );
    }

    if (!ImGui::BeginPopupModal(ModalId, nullptr, Flags)) {
        // Only need to call EndPopup if begin BeginPopupModal returns true
        return;
    }

    static const char *const dont_save_str = "Don't save"; // Longest string
    const ImVec2 size = {
        ImGui::CalcTextSize(dont_save_str).x + 2 * ImGui::GetStyle().ItemInnerSpacing.x,
        0,
    };

    // TODO: [FIXME(?)] the below is a bit of a hack, since currently we can't save an annotation
    // that is in an invalid state. Ideally, would be able to save invalid annotations to a
    // intermediary store so they can be restored.
    const auto *task_view =
        std::get_if<app::ActiveSwallowLabellingTaskView>(&m_app.active_task_view());
    const bool can_save = task_view ? task_view->can_save_annotation() : false;

    ImGui::Text("There are unsaved annotation changes!");
    if (can_save) {
        ImGui::Text("Do you want to save these changes?");
    }
    ImGui::Spacing();

    if (can_save) {
        if (ImGui::Button("Save", size)) {
            m_app.save_unsaved_task_and_switch();
            m_unsaved_task_switch_modal_open = false;
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
    }
    if (ImGui::Button(dont_save_str, size)) {
        m_app.discard_unsaved_task_and_switch();
        m_unsaved_task_switch_modal_open = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", size)) {
        m_app.cancel_unsaved_task_switch();
        m_unsaved_task_switch_modal_open = false;
    }
    if (!can_save) {
        ImGui::SetItemDefaultFocus();
    }

    if (!m_unsaved_task_switch_modal_open) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

}; // namespace recap::labeller::gui
