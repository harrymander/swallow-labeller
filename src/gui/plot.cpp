#include "app/config.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS

#include "gui/plot.hpp"
#include "gui/widgets/plot-range-selector.hpp"
#include "gui/widgets/util.hpp"
#include "util/util.hpp"
#include "widgets/plot-range.hpp"

#include <imgui.h>
#include <implot.h>

#include <vector>

namespace recap::labeller::gui {

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

constexpr float TextAutoalignMargin = 15;
constexpr float TextAutoalignPadding = 6;

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
 * Add right-aligned text ending at (xp, yp), automatically left-aligning text if it would
 * extend before xstart
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

void add_plot_marker(ImDrawList *draw_list, const ImVec2& pos)
{
    constexpr float half_width = 4;
    draw_list->AddRect(
        ImVec2(pos.x - half_width, pos.y - half_width),
        ImVec2(pos.x + half_width, pos.y + half_width),
        ImColor(128, 128, 128)
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

void draw_delta_selector(widgets::PlotRangeSelector& selector)
{
    constexpr ImU32 Color = IM_COL32(120, 120, 120, 50);

    selector.update("##delta_selector", 0, ImGuiMouseButton_Right);
    const widgets::PlotRange *range = selector.range();
    if (!range) {
        return;
    }

    widgets::draw_plot_range(*range, Color);
    const ImVec2 plot_pos = ImPlot::GetPlotPos();
    const ImVec2 plot_size = ImPlot::GetPlotSize();
    const float yp = plot_pos.y + plot_size.y / 2;
    const double xrange = range->range();
    const std::string text = fmt::format("Δt = {:g}", xrange);
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

std::size_t
plot_autostrided_line(const char *id, const std::vector<double>& x, const std::vector<double>& y)
{
    constexpr ImPlotLineFlags Flags = 0;
    constexpr int Offset = 0;

    auto xlim = ImPlot::GetPlotLimits().X;
    auto start = binary_search_closest(x.begin(), x.end(), xlim.Min);
    auto end = binary_search_closest(x.begin(), x.end(), xlim.Max);
    auto offset = std::distance(x.begin(), start);
    auto num_points = static_cast<int>(std::distance(start, end));

    int max_num_points = app::get_global_app_config().max_num_plot_points;
    int stride_points = std::max(num_points / max_num_points, 1);
    num_points /= stride_points;
    ImPlot::PlotLine(
        id,
        x.data() + offset,
        y.data() + offset,
        num_points,
        Flags,
        Offset,
        static_cast<int>(sizeof(double)) * stride_points
    );
    return static_cast<std::size_t>(num_points);
}

}; // namespace

std::size_t Plot::draw()
{
    setup_axis_links(ImAxis_X1, m_xrange.start, m_xrange.end);
    ImPlot::SetupAxis(
        ImAxis_Y1, m_ylabel.c_str(), ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit
    );
    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, m_xdata[0], m_xdata.back());

    std::size_t num_points = plot_autostrided_line("##line", m_xdata, m_ydata);
    if (is_mouse_inside_plot()) {
        draw_plot_hovered(m_xdata.data(), m_xdata.size(), m_ydata.data(), m_cursor_format);
    }
    draw_delta_selector(m_delta_selector);
    return num_points;
}

}; // namespace recap::labeller::gui
