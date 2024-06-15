#define IMGUI_DEFINE_MATH_OPERATORS

#include "plotter.hpp"

#include "data.hpp"
#include "drag-range.hpp"
#include "util.hpp"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <iterator>

namespace recap::labeller::plotter {

using plot::SwallowTaskData;

static ImPlotRange initial_range(const SwallowTaskData& data)
{
    constexpr double EventBufferSecs = 6;
    const auto& event = data.event;
    const auto& time = data.flow_time;
    constexpr auto is_non_zero = [](auto e) { return e != 0; };

    const auto& event_start = std::find_if(event.begin(), event.end(), is_non_zero);
    if (event_start == event.end()) {
        return {time.front(), time.back()};
    }
    const double event_start_time = time[std::distance(event.begin(), event_start)];

    const auto& event_end = std::find_if(event.rbegin(), event.rend(), is_non_zero);
    double end_time;
    if (event_end == event.rend()) {
        end_time = time.back();
    } else {
        const double event_end_time = time[std::distance(event.begin(), event_end.base()) - 1];
        end_time = std::min(time.back(), event_end_time + EventBufferSecs);
    }

    return {
        std::max(event_start_time - EventBufferSecs, time.front()),
        end_time,
    };
}

SwallowTaskPlotter::SwallowTaskPlotter(const SwallowTaskData& data) :
    data(data), event(data.event.begin(), data.event.end()), summary_range(initial_range(data))
{}

static bool begin_data_plot(const char *id)
{
    return ImPlot::BeginPlot(
        id, ImVec2(-1, 0), ImPlotFlags_NoMouseText | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMenus
    );
}

void SwallowTaskPlotter::draw(const char *id)
{
    ImGui::PushID(id);

    if (begin_data_plot("Flow")) {
        draw_flow_plot();
        ImPlot::EndPlot();
    }

    if (begin_data_plot("Ear audio")) {
        draw_audio_plot();
        ImPlot::EndPlot();
    }

    if (ImPlot::BeginPlot("##summary", ImVec2(-1, 75), ImPlotFlags_CanvasOnly)) {
        draw_summary_plot();
        ImPlot::EndPlot();
    }

    ImGui::PopID();
}

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

inline void plot_line(const char *id, const std::vector<double>& x, const std::vector<double>& y)
{
    ImPlot::PlotLine(id, x.data(), y.data(), y.size());
}

void SwallowTaskPlotter::plot_event_digital() const
{
    ImPlot::PlotDigital("##event", data.flow_time.data(), event.data(), event.size());
}

void SwallowTaskPlotter::plot_data(const std::vector<double>& x, const std::vector<double>& y)
{
    ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, x[0], x.back());
    setup_axis_links(ImAxis_X1, &summary_range.Min, &summary_range.Max);
    plot_line("##data", x, y);
    plot_event_digital();
    if (mouse_inside_plot()) {
        draw_plot_hovered(x.data(), x.size(), y.data());
    }
}

void SwallowTaskPlotter::draw_flow_plot()
{
    plot_data(data.flow_time, data.flow);
}

void SwallowTaskPlotter::draw_audio_plot()
{
    plot_data(data.audio_time, data.audio);
}

void SwallowTaskPlotter::draw_summary_plot()
{
    constexpr ImPlotAxisFlags ax_flags = ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_AutoFit;
    ImPlot::SetupAxes(nullptr, nullptr, ax_flags, ax_flags);

    constexpr ImColor summary_color = {.5f, .5, .5, .6};
    summary_selector.draw(
        0, summary_range, summary_color, plot::PlotXSelector::NoCursor, ImGuiMouseButton_Left
    );
    if (!summary_selector.is_selecting()) {
        plot::drag_xrange(0, summary_range, summary_color);
    }

    plot_line("##summary_flow", data.flow_time, data.flow);
    plot_event_digital();
}

}; // namespace recap::labeller::plotter
