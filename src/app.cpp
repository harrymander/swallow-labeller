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

static void draw_plot_contents()
{
    static PlotData data;
    ImPlot::PlotLine("##data", data.x.data(), data.y.data(), data.size);
    if (ImPlot::IsPlotHovered()) {
        const auto mouse_pos = ImPlot::GetPlotMousePos();
        const float x = ImPlot::PlotToPixels(mouse_pos).x;
        ImDrawList *draw_list = ImPlot::GetPlotDrawList();
        const float top = ImPlot::GetPlotPos().y;
        const float bottom = top + ImPlot::GetPlotSize().y;
        draw_list->AddLine(ImVec2(x, top), ImVec2(x, bottom), ImColor(128, 128, 128));
        if (mouse_pos.x >= data.x[0]) {
            const auto iter = std::lower_bound(data.x.begin(), data.x.end(), mouse_pos.x);
            if (iter != data.x.end()) {
                const auto yp = data.y[std::distance(data.x.begin(), iter)];
                if (ImGui::BeginTooltip()) {
                    ImGui::Text("%.3f", yp);
                    ImGui::EndTooltip();
                }
                draw_list->AddCircle(
                    ImVec2(x, ImPlot::PlotToPixels(0, yp).y), 6, ImColor(128, 128, 128), 0, 1
                );
            }
        }
    }
}

static void draw_plot()
{
    if (ImPlot::BeginPlot("Tooltip demo")) {
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
