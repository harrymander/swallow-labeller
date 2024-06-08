#include "app.hpp"

#include "dragger.hpp"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <optional>
#include <sstream>
#include <vector>

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
    using Vector = std::vector<float>;
    Vector y;
    Vector x;
    const Vector::size_type size;

    explicit PlotData(Vector::size_type size) : size(size)
    {
        y.reserve(size);
        x.reserve(size);
        for (std::size_t i = 0; i < size; i++) {
            const float xi = i * 1.0 / (size - 1);
            x.push_back(xi);
            y.push_back(0.25f + 0.25f * sinf(25 * xi) * sinf(5 * xi));
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

static void draw_plot_hovered(const PlotData& data)
{
    const auto mouse = ImPlot::GetPlotMousePos();
    if (mouse.x > data.x[0]) {
        const auto xplot = binary_search_closest(data.x.begin(), data.x.end(), mouse.x);
        if (xplot != data.x.end())
            draw_plot_cursor(*xplot, data.y[std::distance(data.x.begin(), xplot)]);
    }
}

static void draw_plot_vspan(double xmin, double xmax, const ImColor& color)
{
    const auto yrange = ImPlot::GetPlotLimits().Y;
    ImPlot::GetPlotDrawList()->AddRectFilled(
        ImPlot::PlotToPixels(xmin, yrange.Min), ImPlot::PlotToPixels(xmax, yrange.Max), color
    );
}

static void draw_plot_vspan(const ImPlotRange& xrange, const ImColor& color)
{
    draw_plot_vspan(xrange.Min, xrange.Max, color);
}

static void draw_plot()
{
    static PlotData data(1001);
    static plot::PlotRangeDragger zoom_rect_dragger;
    static ImPlotRange zoom_rect(data.x[data.size / 4], data.x[data.size * 3 / 4]);

    static plot::PlotRangeDraggerFlags dragger_flags = 0;
    ImGui::CheckboxFlags("No create", &dragger_flags, plot::PlotRangeDragger::NoCreate);
    ImGui::SameLine();
    ImGui::CheckboxFlags("No resize", &dragger_flags, plot::PlotRangeDragger::NoResize);
    ImGui::SameLine();
    ImGui::CheckboxFlags("No move", &dragger_flags, plot::PlotRangeDragger::NoMove);

    static bool ctrl_for_create = false;
    ImGui::Checkbox("Ctrl for create", &ctrl_for_create);

    if (ImPlot::BeginPlot(
            "##mainplot", ImVec2(-1, 0), ImPlotFlags_NoMouseText | ImPlotFlags_NoBoxSelect
        ))
    {
        ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
        ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, data.x[0], data.x[data.size - 1]);
        ImPlot::SetupAxisLinks(ImAxis_X1, &zoom_rect.Min, &zoom_rect.Max);
        ImPlot::PlotLine("##data", data.x.data(), data.y.data(), data.size);
        if (ImPlot::IsPlotHovered())
            draw_plot_hovered(data);
        ImPlot::EndPlot();
    }

    if (ImPlot::BeginPlot("##summary", ImVec2(-1, 75), ImPlotFlags_CanvasOnly)) {
        static constexpr ImPlotAxisFlags axis_flags =
            ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_AutoFit;
        ImPlot::SetupAxes(nullptr, nullptr, axis_flags, axis_flags);
        draw_plot_vspan(zoom_rect, {128, 128, 128, 100});
        zoom_rect_dragger.draw_update(
            zoom_rect, dragger_flags, ctrl_for_create ? ImGuiKey_LeftCtrl : ImGuiKey_None
        );
        ImPlot::PlotLine("##data", data.x.data(), data.y.data(), data.size);
        ImPlot::EndPlot();
    }
}

static void draw_large_data_plot(const PlotData& data)
{
    if (ImPlot::BeginPlot("##largeplot"), ImVec2(-1, 0), ImPlotFlags_NoBoxSelect) {
        ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
        ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, data.x[0], data.x[data.size - 1]);

        const auto xlimits = ImPlot::GetPlotLimits().X;
        auto xmin = binary_search_closest(data.x.begin(), data.x.end(), xlimits.Min);
        if (xmin == data.x.end())
            xmin = data.x.begin();
        auto xmax = binary_search_closest(data.x.begin(), data.x.end(), xlimits.Max);
        const size_t downsample = data.size / 10'000 + 1;
        const auto imin = xmin - data.x.begin();
        ImPlot::PlotStairs(
            "##data",
            &data.x.data()[imin],
            &data.y.data()[imin],
            (xmax - xmin) / downsample,
            0,
            0,
            sizeof(PlotData::Vector::value_type) * downsample
        );
        ImPlot::EndPlot();
    }
}

static void draw_large_data_plot()
{
    static std::optional<PlotData> data = std::nullopt;
    static std::optional<std::future<PlotData>> future = std::nullopt;
    static constexpr size_t large_data_size = 67'982'231;
    static std::string button_label = []() {
        std::stringstream ss;
        ss << "Load large data (~" << large_data_size / (1 << 20) + 1 << " MiB)";
        return ss.str();
    }();

    if (data) {
        draw_large_data_plot(*data);
    } else if (future.has_value()) {
        if (future->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            data.emplace(future->get());
            future.reset();
        } else {
            static constexpr const char *dots[] = {"", ".", "..", "..."};
            ImGui::Text("Loading%s", dots[(int) (ImGui::GetTime() / .25f) & 3]);
        }
    } else if (ImGui::Button(button_label.c_str())) {
        future.emplace(std::async(std::launch::async, []() { return PlotData(large_data_size); }));
    }
}

static void draw_window_contents()
{
    draw_demo_windows();
    draw_plot();
    draw_large_data_plot();
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
