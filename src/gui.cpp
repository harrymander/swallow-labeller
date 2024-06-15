#include "gui.hpp"

#include <cstddef>
#include <fstream>
#include <stdexcept>
#define IMGUI_DEFINE_MATH_OPERATORS

#include "data.hpp"
#include "drag-range.hpp"
#include "labelling-task.hpp"
#include "selector.hpp"
#include "util.hpp"

#include <cnpy.h>
#include <imgui.h>
#include <implot.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

namespace recap::labeller::gui {

using labelling_task::SwallowLabellingTask;

static void add_plot_marker(ImDrawList *draw_list, const ImVec2& pos)
{
    constexpr float half_width = 4;
    draw_list->AddRect(
        ImVec2(pos.x - half_width, pos.y - half_width),
        ImVec2(pos.x + half_width, pos.y + half_width),
        ImColor(128, 128, 128)
    );
}

/**
 * Add text in position (xp, yp), automatically right-aligining text if it would be greater than
 * xend
 */
static void
add_text_autoalign(ImDrawList *draw_list, const char *text, float xp, float yp, float xend)
{
    constexpr float align_margin = 15;
    constexpr float padding = 6;
    const auto text_size = ImGui::CalcTextSize(text);
    if (xp + text_size.x + align_margin > xend) {
        xp -= text_size.x + padding;
    } else {
        xp += padding;
    }
    draw_list->AddText(ImVec2(xp, yp), ImGui::GetColorU32(ImGuiCol_Text), text);
}

static void add_plot_vline(ImDrawList *draw_list, const ImVec2& posplot, const ImVec2& pospx)
{
    const ImVec2 plot_pos = ImPlot::GetPlotPos();
    const ImVec2 plot_size = ImPlot::GetPlotSize();
    const ImVec2 top(pospx.x, plot_pos.y);
    const ImVec2 bottom(pospx.x, top.y + plot_size.y);
    draw_list->AddLine(top, bottom, ImColor(128, 128, 128));

    const float xend = plot_pos.x + plot_size.x;
    char xtext[20];
    std::snprintf(xtext, sizeof(xtext), "x=%g", posplot.x);
    add_text_autoalign(
        draw_list, xtext, bottom.x, bottom.y - ImGui::GetTextLineHeightWithSpacing(), xend
    );

    char ytext[20];
    std::snprintf(ytext, sizeof(ytext), "y=%g", posplot.y);
    add_text_autoalign(draw_list, ytext, top.x, top.y, xend);
}

static void draw_plot_cursor(float xplot, float yplot)
{
    ImDrawList *draw_list = ImPlot::GetPlotDrawList();
    const auto pospx = ImPlot::PlotToPixels(xplot, yplot);
    add_plot_vline(draw_list, ImVec2(xplot, yplot), pospx);
    add_plot_marker(draw_list, pospx);
}

static void draw_plot_hovered(const double *x, size_t n, const double *y)
{
    const auto mouse = ImPlot::GetPlotMousePos();
    if (mouse.x > x[0]) {
        const double *const end = x + n;
        const double *xclosest = util::binary_search_closest(x, end, mouse.x);
        if (xclosest != end)
            draw_plot_cursor(*xclosest, y[xclosest - x]);
    }
}

inline const char *bool_string(bool val)
{
    return val ? "true" : "false";
}

struct DragXRange {
    DragXRange(double xmin, double xmax, const ImColor& color) : range(xmin, xmax), color(color) {}

    bool draw(ImGuiID id) { return draw_with_flag(id, flags); }

    bool draw_no_input(ImGuiID id)
    {
        return draw_with_flag(id, flags | plot::DragXRangeFlag::NoInput);
    }

    void draw_info_text() const
    {
        ImGui::Text(
            "[%lf, %lf], clicked = %s, hovered = %s, held = %s",
            range.Min,
            range.Max,
            bool_string(clicked),
            bool_string(hovered),
            bool_string(held)
        );
    }

    ImPlotRange range;
    ImColor color;
    plot::DragXRangeFlags flags = 0;

    bool clicked = false;
    bool hovered = false;
    bool held = false;

private:
    bool draw_with_flag(ImGuiID id, plot::DragXRangeFlags flags)
    {
        return plot::drag_xrange(id, range, color, flags, &clicked, &hovered, &held);
    }
};

static void setup_axis_links(ImAxis axis, double *v1, double *v2)
{
    double *vmin;
    double *vmax;
    std::tie(vmin, vmax) = util::minmax_pointers(v1, v2);
    ImPlot::SetupAxisLinks(axis, vmin, vmax);
}

static bool mouse_inside_plot()
{
    if (!ImGui::IsMousePosValid()) {
        return false;
    }

    const ImVec2 bbmin = ImPlot::GetPlotPos();
    const ImVec2 bbmax = bbmin + ImPlot::GetPlotSize();
    const ImVec2 pos = ImGui::GetMousePos();
    return pos.x >= bbmin.x && pos.x <= bbmax.x && pos.y >= bbmin.y && pos.y <= bbmax.y;
}

class Gui::Impl {
public:
    Impl(const std::vector<SwallowLabellingTask>& tasks, const std::filesystem::path& data_dir) :
        data_dir(data_dir), tasks(tasks)
    {
        spdlog::debug("Setting up ImGui and ImPlot...");
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    }

    Impl(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl& operator=(Impl&&) = delete;

    ~Impl()
    {
        spdlog::debug("Tearing down ImGui and ImPlot...");
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

    void stop() { to_close = true; }

    bool draw()
    {
        const auto& io = ImGui::GetIO();
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::Begin(
            "##mainwindow",
            nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus
        );
        draw_window_contents();
        ImGui::End();
        return !to_close;
    }

private:
    plot::SwallowTaskData load_task(const SwallowLabellingTask& task) const
    {
        spdlog::stopwatch stopwatch;
        const auto path = data_dir / std::filesystem::path(task.npz_file.path);
        const auto ret = plot::SwallowTaskData::from_numpy(cnpy::npz_load(path));
        spdlog::debug(
            "Data loaded from '{}' in {} ms", path.string(), stopwatch.elapsed_ms().count()
        );
        spdlog::debug(
            "{}: flow size = {}, audio size = {}", path.string(), ret.flow.size(), ret.audio.size()
        );
        return ret;
    }

    void draw_task_selector(plot::SwallowTaskData& task)
    {
        std::size_t new_index = current_task_index;
        if (ImGui::ArrowButton("Prev task", ImGuiDir_Left)) {
            new_index = current_task_index ? current_task_index - 1 : tasks.size() - 1;
        }
        ImGui::SameLine();
        if (ImGui::ArrowButton("Next task", ImGuiDir_Right)) {
            new_index = (current_task_index + 1) % tasks.size();
        }
        if (new_index != current_task_index) {
            task = load_task(tasks[new_index]);
            current_task_index = new_index;
        }
    }

    void draw_plot()
    {
        static auto task = load_task(tasks[0]);
        draw_task_selector(task);

        // FIXME: do not create this every loop
        std::vector<double> event(task.event.begin(), task.event.end());

        static plot::PlotXSelector selector;
        static ImPlotRange selector_range;
        static ImPlotRange last_selector_range = {NAN, NAN};

        static bool ctrl_for_create = true;
        static bool right_mouse_for_create = false;
        static plot::PlotSelectorFlags selector_flags = 0;
        ImGui::TextUnformatted("Selector options:");
        ImGui::SameLine();
        ImGui::Checkbox("Ctrl for create", &ctrl_for_create);
        ImGui::SameLine();
        ImGui::Checkbox("Right mouse for create", &right_mouse_for_create);
        ImGui::SameLine();
        ImGui::CheckboxFlags(
            "No cursor##selector_flags", &selector_flags, plot::PlotXSelector::NoCursor
        );
        ImGui::SameLine();
        ImGui::CheckboxFlags(
            "Cancel on key release", &selector_flags, plot::PlotXSelector::CancelOnKeyRelease
        );

        static plot::DragXRangeFlags drag_flags = 0;
        ImGui::TextUnformatted("Drag xrange options:");
        ImGui::SameLine();
        ImGui::CheckboxFlags("No cursor##drag_flags", &drag_flags, plot::DragXRangeFlag::NoCursor);
        ImGui::SameLine();
        ImGui::CheckboxFlags("No move", &drag_flags, plot::DragXRangeFlag::NoMove);
        ImGui::SameLine();
        ImGui::CheckboxFlags("No input", &drag_flags, plot::DragXRangeFlag::NoInput);

        static std::array<DragXRange, 3> drag_ranges = {
            DragXRange(.45, .6, ImColor(255, 0, 0, 60)),
            DragXRange(.7, .8, ImColor(0, 255, 0, 60)),
            DragXRange(.1, .2, ImColor(0, 0, 255, 60)),
        };
        static ImPlotRange summary_range(
            task.audio_time[task.audio_time.size() / 4],
            task.audio_time[task.audio_time.size() * 3 / 4]
        );

        if (ImPlot::BeginPlot(
                "##mainplot",
                ImVec2(-1, 0),
                ImPlotFlags_NoMouseText | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMenus
            ))
        {
            ImPlot::SetupAxis(
                ImAxis_Y1, nullptr, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit
            );
            ImPlot::SetupAxisLimitsConstraints(
                ImAxis_X1, task.audio_time[0], task.audio_time.back()
            );
            setup_axis_links(ImAxis_X1, &summary_range.Min, &summary_range.Max);
            ImPlot::PlotLine(
                "##audio", task.audio_time.data(), task.audio.data(), task.audio_time.size()
            );
            ImPlot::PlotDigital("##event", task.flow_time.data(), event.data(), event.size());
            if (mouse_inside_plot()) {
                draw_plot_hovered(
                    task.audio_time.data(), task.audio_time.size(), task.audio.data()
                );
            }

            if (selector.draw(
                    0,
                    selector_range,
                    plot::PlotXSelector::DefaultColor,
                    selector_flags,
                    right_mouse_for_create ? ImGuiMouseButton_Right : ImGuiMouseButton_Left,
                    ctrl_for_create ? ImGuiKey_LeftCtrl : ImGuiKey_None
                ))
            {
                last_selector_range = selector_range;
            }
            for (unsigned int i = 0; i < drag_ranges.size(); i++) {
                drag_ranges[i].draw(i);
            }
            ImPlot::EndPlot();
        }

        if (ImPlot::BeginPlot("##summary", ImVec2(-1, 75), ImPlotFlags_CanvasOnly)) {
            constexpr ImPlotAxisFlags ax_flags =
                ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_AutoFit;
            ImPlot::SetupAxes(nullptr, nullptr, ax_flags, ax_flags);

            unsigned int xrange_id;
            for (xrange_id = 0; xrange_id < drag_ranges.size(); xrange_id++) {
                drag_ranges[xrange_id].draw_no_input(xrange_id + 1);
            }
            if (selector.is_selecting()) {
                plot::drag_xrange(
                    xrange_id++,
                    selector_range,
                    plot::PlotXSelector::DefaultColor,
                    plot::DragXRangeFlag::NoInput
                );
            }

            constexpr ImColor summary_color = {.5f, .5, .5, .6};
            static plot::PlotXSelector summary_selector;
            summary_selector.draw(
                0,
                summary_range,
                summary_color,
                plot::PlotXSelector::NoCursor,
                ImGuiMouseButton_Left
            );
            if (!summary_selector.is_selecting()) {
                plot::drag_xrange(0, summary_range, summary_color);
            }

            ImPlot::PlotLine(
                "##data", task.audio_time.data(), task.audio.data(), task.audio.size()
            );
            ImPlot::PlotDigital("##event", task.flow_time.data(), event.data(), event.size());
            ImPlot::EndPlot();
        }
        ImGui::Text("Summary range: [%f, %f]", summary_range.Min, summary_range.Max);
        ImGui::Text(
            "Selector: [%lf, %lf], selecting = %s, last selection: [%lf, %lf]",
            selector_range.Min,
            selector_range.Max,
            bool_string(selector.is_selecting()),
            last_selector_range.Min,
            last_selector_range.Max
        );

        for (auto& range : drag_ranges) {
            range.flags = drag_flags;
            range.draw_info_text();
        }

        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("Mouse Position: [%.0f,%.0f]", io.MousePos.x, io.MousePos.y);
    }

    void draw_window_contents()
    {
        draw_demo_windows();
        draw_plot();
    }

    void draw_demo_windows()
    {
        ImGui::Checkbox("ImGui demo window", &show_imgui_demo);
        ImGui::SameLine();
        ImGui::Checkbox("ImPlot demo window", &show_implot_demo);
        if (show_imgui_demo) {
            ImGui::ShowDemoWindow(&show_imgui_demo);
        }
        if (show_implot_demo) {
            ImPlot::ShowDemoWindow(&show_implot_demo);
        }
    }

    std::filesystem::path data_dir;
    std::vector<SwallowLabellingTask> tasks;
    bool to_close = false;
    bool show_implot_demo = false;
    bool show_imgui_demo = false;
    std::size_t current_task_index = 0;
};

Gui::Gui(const std::vector<SwallowLabellingTask>& tasks, const std::filesystem::path& data_dir) :
    pimpl(std::make_unique<Impl>(tasks, data_dir))
{}

Gui::~Gui() = default;

bool Gui::draw()
{
    return pimpl->draw();
}

void Gui::stop()
{
    pimpl->stop();
}

}; // namespace recap::labeller::gui
