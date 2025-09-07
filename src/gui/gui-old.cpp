#include "app/annotation-store.hpp"
#include "models/annotation.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS

#include "app/app.hpp"
#include "app/id-list.hpp"
#include "gui.hpp"
#include "gui/font.hpp"
#include "gui/icons.h"
#include "gui/task-filter.hpp"
#include "gui/widgets/color-scheme-selector.hpp"
#include "gui/widgets/enum-radio-button.hpp"
#include "gui/widgets/plot-range-dragger.hpp"
#include "gui/widgets/plot-range-selector.hpp"
#include "gui/widgets/plot-range.hpp"
#include "gui/widgets/util.hpp"
#include "models/task-info.hpp"
#include "models/time-range.hpp"
#include "util/optutil.hpp"
#include "util/os.hpp"
#include "util/util.hpp"
#include "util/variant-visitor.hpp"

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <implot.h>
#include <nfd.h>
#include <nfd.hpp>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <variant>

constexpr float LabelSummaryHeight = 8; // Same as default ImPlotStyle::DigitalBitHeight

#define SAVE_SHORTCUT_STR "Ctrl+S"
constexpr ImGuiKeyChord SaveShortcutKeyChord = ImGuiMod_Ctrl | ImGuiKey_S;

namespace recap::labeller::gui {

namespace {

struct TimeRangeLabelRegionColors {
    ImColor unselected;
    ImColor hovered;
    ImColor selected;
};

struct GuiColors {
    // Generated using
    // http://www.workwithcolor.com/hsl-color-schemer-01.htm?cp=FC655A&ch=4-96-67&cm=0&sm=4&mil=0&dst=51

    using RGB = std::tuple<uint8_t, uint8_t, uint8_t>;

    static constexpr RGB EventLabelColor = {0xFC, 0x65, 0x5A};
    static constexpr RGB EarClickLabelColor = {0xFC, 0x5A, 0xE1};
    static constexpr RGB NonRespFlowLabelColor = {0x8D, 0x5A, 0xFC};

    // static constexpr ImU32
    // apnea_label_color(app::SwallowApneaAnnotationStatus status, uint8_t alpha = 0xff)
    // {
    //     using enum app::SwallowApneaAnnotationStatus;
    //     switch (status) {
    //     case ExEx:
    //         return color({0xFC, 0xEE, 0x5A}, alpha);
    //     case ExIn:
    //         return color({0x80, 0xFC, 0x5A}, alpha);
    //     case InEx:
    //         return color({0x5A, 0xFC, 0xBE}, alpha);
    //     case InIn:
    //         break;
    //     default:
    //         spdlog::error("apnea_label_color: invalid SwallowApneaAnnotationStatus!");
    //         break;
    //     }
    //     return color({0x5A, 0xB0, 0xFC}, alpha);
    // }

    static constexpr ImU32 color(const RGB& rgb, uint8_t alpha = 0xff)
    {
        return IM_COL32(std::get<0>(rgb), std::get<1>(rgb), std::get<2>(rgb), alpha);
    }

    static constexpr TimeRangeLabelRegionColors time_range_label_region_colors(RGB rgb)
    {
        return {
            .unselected = color(rgb, 0x33),
            .hovered = color(rgb, 0x44),
            .selected = color(rgb, 0x66),
        };
    }
};

void plot_line(const char *id, const std::vector<double>& x, const std::vector<double>& y)
{
    ImPlot::PlotLine(id, x.data(), y.data(), static_cast<int>(y.size()));
}

void plot_event(const models::TimeRange& range)
{
    constexpr ImU32 color = GuiColors::color(GuiColors::EventLabelColor);
    widgets::draw_plot_range(range.start, range.end, color, -LabelSummaryHeight);
}

class Plotter {
private:
    static bool is_mouse_inside_plot()
    {
        if (!ImGui::IsMousePosValid()) {
            return false;
        }

        const ImVec2 bbmin = ImPlot::GetPlotPos();
        const ImVec2 bbmax = bbmin + ImPlot::GetPlotSize();
        const ImVec2 pos = ImGui::GetMousePos();
        return pos.x >= bbmin.x && pos.x <= bbmax.x && pos.y >= bbmin.y && pos.y <= bbmax.y;
    }

    static void add_plot_marker(ImDrawList *draw_list, const ImVec2& pos)
    {
        constexpr float half_width = 4;
        draw_list->AddRect(
            ImVec2(pos.x - half_width, pos.y - half_width),
            ImVec2(pos.x + half_width, pos.y + half_width),
            ImColor(128, 128, 128)
        );
    }

    static constexpr float TextAutoalignMargin = 15;
    static constexpr float TextAutoalignPadding = 6;

    /**
     * Add left-aligned text starting at (xp, yp), automatically right-aligning text if it would be
     * extend past xend
     */
    static void
    add_text_autoalign_left(ImDrawList *draw_list, const char *text, float xp, float yp, float xend)
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
     * Add right-aligned text ending at (xp, yp), automatically left-aligning text if it would
     * extend before xstart
     */
    static void add_text_autoalign_right(
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

    static void add_plot_vline(
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

    static void draw_plot_cursor(double xplot, double yplot, fmt::format_string<double> yfmt)
    {
        ImDrawList *draw_list = ImPlot::GetPlotDrawList();
        const auto pospx = ImPlot::PlotToPixels(xplot, yplot);
        add_plot_vline(draw_list, xplot, yplot, pospx, "t = {:g} s", yfmt);
        add_plot_marker(draw_list, pospx);
    }

    static void
    draw_plot_hovered(const double *x, size_t n, const double *y, fmt::format_string<double> yfmt)
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

    void draw_delta_selector()
    {
        constexpr ImU32 Color = IM_COL32(120, 120, 120, 50);

        m_delta_selector.update("##delta_selector", 0, ImGuiMouseButton_Right);
        const widgets::PlotRange *range = m_delta_selector.range();
        if (!range) {
            return;
        }

        widgets::draw_plot_range(*range, Color);
        const ImVec2 plot_pos = ImPlot::GetPlotPos();
        const ImVec2 plot_size = ImPlot::GetPlotSize();
        const float yp = plot_pos.y + plot_size.y / 2;
        const double xrange = range->range();
        const std::string text = fmt::format("Δt = {:.g}", xrange);
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

public:
    Plotter(
        std::string ylabel, fmt::format_string<double> cursor_format, widgets::PlotRange& xrange
    ) :
        m_ylabel(std::move(ylabel)), m_cursor_format(cursor_format), m_xrange(xrange)
    {}

    template <typename DrawFunc> void draw(const char *id, float height, DrawFunc&& draw)
    {
        constexpr ImPlotFlags Flags =
            ImPlotFlags_NoMouseText | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMenus;
        widgets::ScopedImID scoped_id(id);
        if (ImPlot::BeginPlot("##plot", {-1, height}, Flags)) {
            draw();
            ImPlot::EndPlot();
        }
    }

    static void setup_axis_links(ImAxis axis, double& v1, double& v2)
    {
        if (v1 <= v2) {
            ImPlot::SetupAxisLinks(axis, &v1, &v2);
        } else {
            ImPlot::SetupAxisLinks(axis, &v2, &v1);
        }
    }

    void plot_data(const std::vector<double>& x, const std::vector<double>& y)
    {
        setup_axis_links(ImAxis_X1, m_xrange.start, m_xrange.end);
        ImPlot::SetupAxis(
            ImAxis_Y1, m_ylabel.c_str(), ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit
        );
        ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, x[0], x.back());

        plot_line("##line", x, y);
        if (is_mouse_inside_plot()) {
            draw_plot_hovered(x.data(), x.size(), y.data(), m_cursor_format);
        }
        draw_delta_selector();
    }

private:
    std::string m_ylabel;
    fmt::format_string<double> m_cursor_format;
    widgets::PlotRange& m_xrange;
    widgets::PlotRangeSelector m_delta_selector;
};

}; // namespace

class Gui::Impl {
private:
    static constexpr double FlowMinSelectionRange = 1.0 / 1000;
    static constexpr double AudioMinSelectionRange = FlowMinSelectionRange;

    struct TimeRangeAnnotator {
        widgets::PlotRangeSelector range_selector;
        widgets::PlotRangeDragger range_dragger;
        widgets::PlotRange temp_range = {NAN, NAN};
        std::optional<app::IDList<models::TimeRange>::Item::ID> selected_id = std::nullopt;
        std::optional<app::IDList<models::TimeRange>::Item::ID> hovered_id = std::nullopt;

        TimeRangeAnnotator() = default;
    };

#if NDEBUG
    static constexpr bool DefaultShowDebugInfo = false;
#else
    static constexpr bool DefaultShowDebugInfo = true;
#endif

    bool m_first_draw = true;
    bool m_reset_dockspace = false;
    std::string m_ini_path;
    bool m_show_imgui_demo_window = false;
    bool m_show_implot_demo_window = false;
    bool m_show_imgui_metrics = false;
    bool m_show_debug_info = DefaultShowDebugInfo;
    bool m_critical_error_modal_open = false;
    bool m_unsaved_task_switch_modal_open = false;
    recap::labeller::gui::widgets::ColorSchemeSelector m_color_scheme_selector;

    TaskFilter m_task_filter;
    std::size_t m_task_list_last_active_index = std::numeric_limits<std::size_t>::max();

    widgets::PlotRangeDragger m_plot_summary_dragger;
    widgets::PlotRangeSelector m_plot_summary_selector;
    widgets::PlotRange m_plot_summary_range = {NAN, NAN};

    recap::labeller::app::App& m_app;
    recap::labeller::app::App::NewActiveTaskObservable::Observer m_new_active_task_observer;
    Plotter m_flow_plotter;
    Plotter m_audio_plotter;

    std::future<os::OsOpenStatus> m_open_annotations_path_future;
    bool m_nfd_available = true;

    static bool is_valid_ini_path(const std::filesystem::path& path)
    {
        namespace fs = std::filesystem;
        if (fs::is_directory(path)) {
            spdlog::error("Invalid INI path: '{}' is a directory", path);
            return false;
        }

        const auto parent = path.parent_path();
        if (!parent.empty() && !fs::is_directory(parent)) {
            spdlog::error(
                "Invalid INI path: parent directory '{}' does not exist or is not a directory",
                parent
            );
            return false;
        }

        return true;
    }

    void setup_imgui_ini()
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

    void on_new_active_task(const app::App::ActiveTaskVariant& new_task)
    {
        constexpr double InitViewRangeMargin = 0.025;

        VariantVisitor{
            [this](const app::ActiveSwallowLabellingTaskView& task) {
                const auto& time = task.data().flow_time;
                const double margin = (time.back() - time.front()) * InitViewRangeMargin;
                m_plot_summary_range = {time.front() + margin, time.back() - margin};
                spdlog::debug(
                    "Set new summary range to [{}, {}]",
                    m_plot_summary_range.start,
                    m_plot_summary_range.end
                );
            },
            [this](const app::ActiveSwallowLabellingTaskErrorView&) {
                m_plot_summary_selector.reset();
                m_plot_summary_range = {NAN, NAN};
            },
        }(new_task);
    }

    void draw_critical_error(const std::string& error)
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

    void setup_dockspace()
    {
        constexpr ImGuiDockNodeFlags DockspaceFlags = ImGuiDockNodeFlags_AutoHideTabBar;

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
        if (m_first_draw || m_reset_dockspace) [[unlikely]] {
            const bool configure = m_reset_dockspace || ImGui::DockBuilderGetNode(id) == nullptr;
            m_reset_dockspace = false;
            ImGui::DockSpaceOverViewport(id, viewport, DockspaceFlags);
            if (configure) {
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

    static void draw_status_bar()
    {
        constexpr float FramePadding = 5;
        widgets::ScopedImStyle style_scope = {
            {ImGuiStyleVar_WindowBorderSize, 0.0F},
            {ImGuiStyleVar_WindowPadding, ImVec2{ImGui::GetStyle().WindowPadding.x, 0}},
            {ImGuiStyleVar_FramePadding, ImVec2{FramePadding, FramePadding}},
        };

        constexpr ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;
        if (!ImGui::BeginViewportSideBar(
                "##viewport_status_bar",
                ImGui::GetMainViewport(),
                ImGuiDir_Down,
                ImGui::GetFrameHeight(),
                WindowFlags
            ))
        {
            return;
        }

        ImGui::AlignTextToFramePadding();
        const ImGuiIO& io = ImGui::GetIO();
        ImGui::Text(
            DEBUG_INFO_ICON ICON_TEXT_SPACE
            "Mouse Position: [%.0f,%.0f]. Application average: %.3f ms/frame (%.1f FPS).",
            io.MousePos.x,
            io.MousePos.y,
            1000.0f / io.Framerate,
            io.Framerate
        );

        ImGui::End();
    }

    void draw_main_window()
    {
        const auto& task = m_app.tasks().at(m_app.active_task_index());
        const auto& info = task.info();
        ImGui::Text(
            "Subject #%u, %s swallows, repeat #%u (%s)",
            info.subject,
            swallow_test_type_string(info.test_type).c_str(),
            info.repeatnum,
            task.data_path().c_str()
        );

        VariantVisitor{
            [this](app::ActiveSwallowLabellingTaskView& task) { draw_plots(task); },
            [this](const app::ActiveSwallowLabellingTaskErrorView& error) {
                ImGui::Text(ERR_ICON ICON_TEXT_SPACE "%s", error.error_msg().c_str());
                if (ImGui::Button("Go to next unannotated task" ICON_TEXT_SPACE SKIP_TASK_ICON)) {
                    m_app.go_to_next_unannotated_task();
                }
                ImGui::SameLine();
                if (ImGui::Button("Retry...")) {
                    m_app.reload_active_task();
                }
            },
        }(m_app.active_task_variant());
    }

    void draw_plots(app::ActiveSwallowLabellingTaskView& task_view)
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
            constexpr ImPlotAxisFlags AxFlags =
                ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_AutoFit;
            ImPlot::SetupAxes(nullptr, nullptr, AxFlags, AxFlags);
            draw_plot_summary_selector();
            const auto& data = task_view.data();
            plot_line("##summary_flow_plot_line", data.flow_time, data.flow);
            // TODO: plot event button pushes
            ImPlot::EndPlot();
        }
    }

    // Add text in top left corner of plot
    static void add_plot_text(const char *str, unsigned int offset_lines = 0)
    {
        const ImVec2 pos = ImPlot::GetPlotPos() + ImGui::GetStyle().ItemSpacing;
        ImPlot::PushPlotClipRect();
        ImPlot::GetPlotDrawList()->AddText(
            {
                pos.x,
                pos.y + ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(offset_lines),
            },
            ImPlot::GetStyleColorU32(ImPlotCol_InlayText),
            str
        );
        ImPlot::PopPlotClipRect();
    }

    void draw_flow_plot(app::ActiveSwallowLabellingTaskView& task_view)
    {
        const auto& data = task_view.data();
        m_flow_plotter.plot_data(data.flow_time, data.flow);
        add_plot_text("Positive flow = expiration");
    }

    void draw_audio_plot(app::ActiveSwallowLabellingTaskView& task_view)
    {
        const auto& data = task_view.data();
        m_audio_plotter.plot_data(data.audio_time, data.audio);
    }

    static void draw_labels_regions(
        const std::vector<app::TimeRangeIDList::Item>& labels,
        const TimeRangeAnnotator& annotator,
        float height,
        const TimeRangeLabelRegionColors& colors,
        bool selected_color
    )
    {
        for (const auto& label : labels) {
            if (annotator.range_dragger.is_editing() && annotator.selected_id == label.id)
                [[unlikely]]
            {
                widgets::draw_plot_range(annotator.temp_range, colors.selected, height);
            } else {
                const auto& color = selected_color || annotator.selected_id == label.id ?
                    colors.selected :
                    (annotator.hovered_id == label.id ? colors.hovered : colors.unselected);
                widgets::draw_plot_range(label.item.start, label.item.end, color, height);
            }
        }

        const auto *selector_range = annotator.range_selector.range();
        if (selector_range) {
            widgets::draw_plot_range(
                *selector_range, selected_color ? colors.selected : colors.unselected, height
            );
        }
    }

    void draw_plot_summary_selector()
    {
        constexpr ImColor SummaryColor = {.5F, .5F, .5F, .6F};

        m_plot_summary_selector.update("##plot_summary_selector");
        const widgets::PlotRange *new_range = m_plot_summary_selector.range();
        if (new_range) {
            m_plot_summary_range = *new_range;
        } else {
            m_plot_summary_dragger.update("##plot_summary_dragger", m_plot_summary_range);
        }
        widgets::draw_plot_range(m_plot_summary_range, SummaryColor);

        const auto *task_view = m_app.active_task_labelling_view();
        if (task_view) {}
    }

    void annotations_save_copy()
    {
        static const std::array<nfdfilteritem_t, 1> save_filters = {{
            {"JSON", "json"},
        }};

        spdlog::info("Selecting annotations file save copy path...");
        NFD::UniquePath save_path;
        nfdresult_t res = NFD::SaveDialog(
            save_path,
            save_filters.data(),
            static_cast<nfdfiltersize_t>(save_filters.size()),
            nullptr,
            "annotations.json"
        );
        if (res == NFD_OKAY) {
            if (save_path) {
                m_app.save_annotations_to_path(save_path.get());
            } else {
                spdlog::error("NFD::SaveDialog returned okay, but path string is null");
            }
        } else if (res == NFD_CANCEL) {
            spdlog::info("Annotations file save copy cancelled");
        } else {
            spdlog::error("Error picking annotations save path: {}", NFD::GetError());
        }
    }

    void annotations_path_open()
    {
        if (m_open_annotations_path_future.valid()) {
            if (m_open_annotations_path_future.wait_for(std::chrono::seconds(0))
                == std::future_status::ready)
            {
                m_open_annotations_path_future.get();
            }
        }
        bool can_open = !m_open_annotations_path_future.valid();
        ImGui::BeginDisabled(!can_open);
        if (ImGui::MenuItem("Open annotations file in explorer...") && can_open) {
            m_open_annotations_path_future =
                os::open_path_in_file_explorer(m_app.annotations_path());
        }
        ImGui::EndDisabled();
    }

    void draw_menu_bar()
    {
        if (ImGui::BeginMenu("File")) {
            ImGui::BeginDisabled(!m_nfd_available);
            if (ImGui::MenuItem("Save a copy of annotations file...") && m_nfd_available) {
                annotations_save_copy();
            }
            ImGui::EndDisabled();
            annotations_path_open();

            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) {
                stop();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Reset window layout")) {
                spdlog::debug("Resetting view...");
                m_reset_dockspace = true;
                m_show_imgui_demo_window = false;
                m_show_implot_demo_window = false;
                m_show_imgui_metrics = false;
                m_show_debug_info = DefaultShowDebugInfo;
            }
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

    static std::string swallow_task_info_str(
        const app::SwallowLabellingTask& task, bool is_annotated, bool has_suggested_annotation
    )
    {
        const char *task_icon = "";
        if (task.error_msg().has_value()) {
            task_icon = FILE_ERR_ICON ICON_TEXT_SPACE;
        } else if (is_annotated) {
            task_icon = ANNOTATED_TASK_ICON ICON_TEXT_SPACE;
        }

        const models::SwallowTaskInfo& info = task.info();
        return fmt::format(
            "{}Subject #{}, {}\nRepeat #{}{}",
            task_icon,
            info.subject,
            swallow_test_type_string(info.test_type),
            info.repeatnum,
            !*task_icon && has_suggested_annotation ?
                ICON_TEXT_SPACE SUGGESTED_ANNOTATION_TASK_ICON :
                ""
        );
    }

    void draw_task_history_controls()
    {
        ImGui::BeginDisabled(!m_app.can_go_to_previous_task());
        if (ImGui::ArrowButton("##prev_task", ImGuiDir_Left)
            || widgets::global_shortcut(ImGuiMod_Alt | ImGuiKey_LeftArrow))
        {
            m_app.go_to_previous_task_in_history();
        }
        ImGui::SetItemTooltip("Go back [Alt+Left]");
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_app.can_go_to_forward_task());
        if (ImGui::ArrowButton("##fwrd_task", ImGuiDir_Right)
            || widgets::global_shortcut(ImGuiMod_Alt | ImGuiKey_RightArrow))
        {
            m_app.go_to_next_task_in_history();
        }
        ImGui::SetItemTooltip("Go forward [Alt+Right]");
        ImGui::EndDisabled();
    }

    void draw_task_list()
    {
        m_task_filter.draw("##task-filter");
        draw_task_history_controls();

        const auto& tasks = m_app.tasks();

        const ImVec2 task_counts_pos = ImGui::GetCursorPos();
        const float task_counts_height =
            ImGui::GetTextLineHeightWithSpacing() * (m_task_filter.enabled() ? 2 : 1);
        ImGui::SetCursorPos({
            task_counts_pos.x,
            task_counts_pos.y + task_counts_height,
        });

        bool scroll_to_selected_task = false;
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32_BLACK_TRANS);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32_BLACK_TRANS);
        ImGui::BeginGroup();
        if (m_app.tasks_shuffled()) {
            if (ImGui::SmallButton(UNSHUFFLE_ICON)) {
                m_app.unshuffle_tasks();
            }
            ImGui::SetItemTooltip("Sort tasks");
        } else {
            if (ImGui::SmallButton(SHUFFLE_ICON)) {
                m_app.shuffle_tasks();
            }
            ImGui::SetItemTooltip("Shuffle tasks");
        }
        const bool scroll_to_top = ImGui::SmallButton(SCROLL_TO_TOP_ICON);
        ImGui::SetItemTooltip("Scroll to top");
        if (ImGui::SmallButton(SCROLL_TO_TASK_ICON)) {
            scroll_to_selected_task = true;
        }
        ImGui::SetItemTooltip("Scroll to active task");
        const bool scroll_to_bottom = ImGui::SmallButton(SCROLL_TO_BOTTOM_ICON);
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
        std::size_t num_filtered = 0;

        std::size_t new_active_index = active_index;
        if (ImGui::BeginListBox("##task_info_list", {-1, -1})) {
            if (scroll_to_top) {
                ImGui::SetScrollHereY();
            }

            std::size_t i = 0;
            for (const auto& task : tasks.items()) {
                const bool selected = active_index == i;
                const auto str = swallow_task_info_str(task, true, true);
                if (ImGui::Selectable(str.c_str(), selected)) {
                    new_active_index = i;
                }
                if (selected && scroll_to_selected_task && !ImGui::IsItemVisible()) {
                    ImGui::ScrollToItem();
                }

                i += 1;
            }

            if (scroll_to_bottom) {
                ImGui::SetScrollHereY();
            }
            ImGui::EndListBox();
        }

        ImGui::SetCursorPos(task_counts_pos);
        if (m_task_filter.enabled()) {
            ImGui::Text(
                FILTER_ICON ICON_TEXT_SPACE "Showing %zu task%s out of %zu",
                num_filtered,
                num_filtered == 1 ? "" : "s",
                tasks.size()
            );
        }
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

    [[nodiscard]] static bool save_shortcut_pushed()
    {
        return widgets::global_shortcut(SaveShortcutKeyChord);
    }

    enum class AnnotationSubmitAction {
        None,
        Save,
        Skip,
    };

    AnnotationSubmitAction draw_annotation_submit(app::ActiveSwallowLabellingTaskView& task_view)
    {
        using enum AnnotationSubmitAction;
        AnnotationSubmitAction action = None;

        if (ImGui::Button("Skip" ICON_TEXT_SPACE SKIP_TASK_ICON) && action == None) {
            action = Skip;
        }
        ImGui::SetItemTooltip("Skip to next unannotated task");
        ImGui::SameLine();
        bool auto_advance = m_app.auto_advance_on_save();
        if (ImGui::Checkbox("Auto-advance to next task on save", &auto_advance)) {
            m_app.set_auto_advance_on_save(auto_advance);
            spdlog::debug("{}abled auto-advance on save", auto_advance ? "En" : "Dis");
        }

        return action;
    }

    static bool delete_button()
    {
        constexpr float ButtonCornerRadius = 5;
        widgets::ScopedImStyle style(ImGuiStyleVar_FrameRounding, ButtonCornerRadius);
        return widgets::ButtonRed(DELETE_ICON);
    }

    static bool delete_button(const char *id)
    {
        widgets::ScopedImID id_scope(id);
        return delete_button();
    }

    template <typename... Args> static bool delete_label_button(Args&&...args)
    {
        const bool clicked = delete_button(std::forward<Args>(args)...);
        ImGui::SetItemTooltip("Delete label");
        return clicked;
    }

    template <typename Delete>
    void draw_labels_list_box(
        const char *name,
        TimeRangeAnnotator& annotator,
        const std::vector<app::TimeRangeIDList::Item>& labels,
        const Delete& deleter
    )
    {
        static const char *remove_button_str = DELETE_ICON;
        constexpr ImVec2 SelectableTextAlign = {0, 0.5};
        widgets::ScopedImStyle selectable_style(
            ImGuiStyleVar_SelectableTextAlign, SelectableTextAlign
        );

        const float text_height = ImGui::GetTextLineHeightWithSpacing();
        widgets::ScopedImID id_scope(name);
        if (!ImGui::BeginListBox("##listbox", {-1, 4 * text_height})) {
            return;
        }

        const float label_width = ImGui::GetContentRegionAvail().x
            - (ImGui::CalcTextSize(remove_button_str).x + 2 * ImGui::GetStyle().ItemSpacing.x);
        const float label_height = text_height;

        std::optional<app::TimeRangeIDList::Item::ID> id_to_remove = std::nullopt;
        annotator.hovered_id.reset();
        for (const auto& label : labels) {
            widgets::ScopedImID label_id_scope(static_cast<int>(label.id));
            const auto& range = label.item;
            std::string str = fmt::format(
                "{} {} [{:.3f}, {:.3f} s] (Δ = {:.3f} s)",
                name,
                label.id,
                range.start,
                range.end,
                range.end - range.start
            );
            const bool selected = optutil::has_value_and_equal(annotator.selected_id, label.id);
            if (ImGui::Selectable(str.c_str(), selected, 0, {label_width, label_height})) {
                if (selected) {
                    spdlog::debug("De-selecting {} label ID={}", name, label.id);
                    annotator.selected_id.reset();
                } else {
                    spdlog::debug("Selecting {} label ID={}", name, label.id);
                    annotator.selected_id = label.id;
                }
            }
            if (ImGui::IsItemHovered()) {
                annotator.hovered_id = label.id;
            }

            ImGui::SameLine();
            if (delete_label_button()) {
                id_to_remove = label.id;
            }
            if (ImGui::IsItemHovered()) {
                annotator.hovered_id = label.id;
            }
        }

        if (id_to_remove.has_value()) {
            deleter(*id_to_remove);
            if (id_to_remove == annotator.selected_id) {
                annotator.selected_id.reset();
            }
            if (id_to_remove == annotator.hovered_id) {
                annotator.hovered_id.reset();
            }
        }

        ImGui::EndListBox();
    }

    void draw_label_editor(app::ActiveSwallowLabellingTaskView& task_view)
    {
        const AnnotationSubmitAction action = draw_annotation_submit(task_view);

        switch (action) {
            using enum AnnotationSubmitAction;
        case Save:
            m_app.save_active_task();
            break;
        case Skip:
            m_app.go_to_next_unannotated_task();
            break;
        case None:
            break;
        }
    }

    void draw_unsaved_task_prompt()
    {
        static const char *ModalId = ERR_ICON ICON_TEXT_SPACE "Unsaved annotation";
        constexpr ImVec2 CentrePos = {0.5F, 0.5F};
        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;

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

        // TODO: [FIXME(?)] the below is a bit of a hack, since currently we can't save an
        // annotation that is in an invalid state. Ideally, would be able to save invalid
        // annotations to a intermediary store so they can be restored.
        ImGui::Text("There are unsaved annotation changes!");
        ImGui::Spacing();

        if (ImGui::Button(dont_save_str, size)) {
            m_app.discard_unsaved_task_and_switch();
            m_unsaved_task_switch_modal_open = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", size) || widgets::global_shortcut(ImGuiKey_Escape)) {
            m_app.cancel_unsaved_task_switch();
            m_unsaved_task_switch_modal_open = false;
        }

        if (!m_unsaved_task_switch_modal_open) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    static constexpr const char *TaskListWindowId = "Task list##tasklistwindow";
    static constexpr const char *MainWindowId = "Labelling##mainwindow";
    static constexpr const char *LabelInfoWindowId = "Labelling info##labelinfowindow";

    template <typename DrawFunc> static void draw_window(const char *id, const DrawFunc& draw)
    {
        constexpr ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        if (ImGui::Begin(id, nullptr, WindowFlags)) {
            draw();
        }
        ImGui::End();
    }

    template <typename ShowFunc> static void show_window(bool& open, const ShowFunc& show)
    {
        if (open) {
            show(&open);
        }
    }

public:
    explicit Impl(app::App& app) :
        m_app(app),
        m_new_active_task_observer(m_app.subscribe_new_active_task([this](const auto& v) {
            on_new_active_task(v);
        })),
        m_flow_plotter("Flow (L/min)", "{:g} L/min", m_plot_summary_range),
        m_audio_plotter("Ear audio (V)", "{:g} V", m_plot_summary_range)
    {
        if (NFD::Init() != NFD_OKAY) {
            spdlog::error("Error initialising NFD: {}", NFD::GetError());
            m_nfd_available = false;
        } else {
            spdlog::debug("NFD initialised");
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
        setup_imgui_ini();

        setup_fonts();

        on_new_active_task(m_app.active_task_variant());
    }

    ~Impl()
    {
        if (m_nfd_available) {
            NFD::Quit();
        }

        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

    Impl(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl& operator=(Impl&&) = delete;

    void draw()
    {
        if (ImGui::BeginMainMenuBar()) {
            draw_menu_bar();
            ImGui::EndMainMenuBar();
        }

        if (m_show_debug_info) {
            draw_status_bar();
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

        auto *task_view = m_app.active_task_labelling_view();
        if (task_view != nullptr) {
            draw_window(LabelInfoWindowId, [this, task_view]() { draw_label_editor(*task_view); });
        }

        show_window(m_show_imgui_demo_window, ImGui::ShowDemoWindow);
        show_window(m_show_imgui_metrics, ImGui::ShowMetricsWindow);
        show_window(m_show_implot_demo_window, ImPlot::ShowDemoWindow);

        m_first_draw = false;
    }

    void stop() { m_app.stop(); }

    [[nodiscard]] bool ready_to_stop() const { return m_app.can_stop(); }
};

Gui::Gui(app::App& app, SwallowAnnotationStore& annotation_store) :
    m_pimpl(std::make_unique<Impl>(app))
{}

Gui::~Gui() = default;

void Gui::draw()
{
    m_pimpl->draw();
}

void Gui::stop()
{
    m_pimpl->stop();
}

void Gui::set_scaling_factor(float scaling_factor)
{
    ImGui::GetStyle().ScaleAllSizes(scaling_factor);
}

bool Gui::ready_to_stop() const
{
    return m_pimpl->ready_to_stop();
}
}; // namespace recap::labeller::gui
