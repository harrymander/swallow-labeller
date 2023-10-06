#include "app.hpp"

#include <imgui.h>
#include <implot.h>

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

static void draw_window_contents()
{
    draw_demo_windows();
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
