#include "app.hpp"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>

namespace app {

static bool to_close = false;

int setup()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    return 0;
}

void teardown()
{
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

static void draw_demo_windows()
{
    static bool show_imgui_demo = false;
    static bool show_implot_demo = false;
    ImGui::Checkbox("ImGui demo window", &show_imgui_demo);
    ImGui::SameLine();
    ImGui::Checkbox("ImPlot demo window", &show_implot_demo);
    if (show_imgui_demo)
        ImGui::ShowDemoWindow(&show_imgui_demo);
    if (show_implot_demo)
        ImPlot::ShowDemoWindow(&show_implot_demo);
}

struct PlotData {
    static constexpr std::size_t size = 1001;
    std::array<float, size> y;
    std::array<float, size> x;

    PlotData()
    {
        for (std::size_t i = 0; i < size; i++) {
            x[i] = i * 0.001f;
            y[i] = 0.25f + 0.25f * sinf(25 * x[i]) * sinf(5 * x[i]);
        }
    }
};

template <class BidirIt, class T>
BidirIt binary_search_closest(BidirIt first, BidirIt last, const T& value)
{
    BidirIt found = std::lower_bound(first, last, value);
    if (found != last && found != first) {
        const auto prev = std::prev(found);
        if (value - *prev < *found - value)
            found = prev;
    }
    return found;
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
    static constexpr float align_margin = 15;
    static constexpr float padding = 6;
    const auto text_size = ImGui::CalcTextSize(text);
    if (xp + text_size.x + align_margin > xend) {
        xp -= text_size.x + padding;
    } else {
        xp += padding;
    }
    draw_list->AddText(ImVec2(xp, yp), ImColor(0xff, 0xff, 0xff), text);
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

    static std::optional<ImPlotRect> rect = std::nullopt;
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)
        && (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)))
    {
        const auto limits = ImPlot::GetPlotLimits();
        if (!rect.has_value()) {
            rect = std::make_optional<ImPlotRect>();
            rect->X.Min = xplot;
        }
        rect->X.Max = xplot;
        rect->Y.Max = limits.Y.Max;
        rect->Y.Min = limits.Y.Min;

        draw_list->AddRectFilled(
            ImPlot::PlotToPixels(rect->Min()),
            ImPlot::PlotToPixels(rect->Max()),
            ImColor(120, 0, 0, 90)
        );
    } else {
        rect.reset();
    }
}

static void draw_plot_contents()
{
    static PlotData data;
    ImPlot::PlotLine("##data", data.x.data(), data.y.data(), data.size);
    if (ImPlot::IsPlotHovered()) {
        const auto mouse = ImPlot::GetPlotMousePos();
        if (mouse.x > data.x[0]) {
            const auto xplot = binary_search_closest(data.x.begin(), data.x.end(), mouse.x);
            if (xplot != data.x.end())
                draw_plot_cursor(*xplot, data.y[std::distance(data.x.begin(), xplot)]);
        }
    }
}

static void draw_plot()
{
    if (ImPlot::BeginPlot(
            "Tooltip demo", ImVec2(-1, 0), ImPlotFlags_NoMouseText | ImPlotFlags_NoBoxSelect
        ))
    {
        draw_plot_contents();
        ImPlot::EndPlot();
    }
}

static void draw_window_contents()
{
    draw_demo_windows();
    draw_plot();
}

bool draw()
{
    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::Begin(
        "##mainwindow",
        nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize
    );
    draw_window_contents();
    ImGui::End();
    return !to_close;
}

void close()
{
    to_close = true;
}

}; // namespace app
