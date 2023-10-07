#include "app.hpp"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

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
private:
    static constexpr std::size_t size_ = 1001;
    std::array<float, size_> xs;
    std::array<float, size_> ys;

public:
    PlotData()
    {
        for (uint16_t i = 0; i < 1001; ++i) {
            xs[i] = i * 0.001f;
            ys[i] = 0.25f + 0.25f * sinf(25 * xs[i]) * sinf(5 * xs[i]);
        }
    }

    constexpr std::size_t size() const { return size_; }

    const decltype(xs)& x() const { return xs; }

    const decltype(ys)& y() const { return ys; }
};

static void draw_plot_contents()
{
    static PlotData data;
    ImPlot::PlotLine("Uncertain Data", data.x().data(), data.y().data(), data.size());
    if (ImPlot::IsPlotHovered()) {
        const auto mouse_pos = ImPlot::GetPlotMousePos();
        const float x = ImPlot::PlotToPixels(mouse_pos).x;
        ImDrawList *draw_list = ImPlot::GetPlotDrawList();
        const float top = ImPlot::GetPlotPos().y;
        const float bottom = top + ImPlot::GetPlotSize().y;
        draw_list->AddLine(ImVec2(x, top), ImVec2(x, bottom), ImColor(128, 128, 128));
        if (ImGui::BeginTooltip()) {
            const auto ind = std::lower_bound(data.x().begin(), data.x().end(), mouse_pos.x);
            if (ind != data.x().end()) {
                const auto i = std::distance(data.x().begin(), ind);
                ImGui::Text("%.3f, %.3f", *ind, data.y()[i]);
            }
            ImGui::EndTooltip();
        }
    }
}

static void draw_plot()
{
    if (ImPlot::BeginPlot("Shaded Plots")) {
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
