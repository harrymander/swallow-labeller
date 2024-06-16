#include "gui.hpp"

#include "data.hpp"
#include "labelling-task.hpp"
#include "plotter.hpp"

#include <cnpy.h>
#include <imgui.h>
#include <implot.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <vector>

namespace recap::labeller::gui {

using labelling_task::SwallowLabellingTask;
using namespace recap::labeller::plotter;

inline const char *bool_string(bool val)
{
    return val ? "true" : "false";
}

class Gui::Impl {
public:
    Impl(const std::vector<SwallowLabellingTask>& tasks, const std::filesystem::path& data_dir) :
        data_dir(data_dir), tasks(tasks), task_plotter(load_current_task())
    {
        spdlog::debug("Setting up ImGui and ImPlot...");
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
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
        const bool should_draw = ImGui::Begin(
            "##mainwindow",
            nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus
        );
        if (should_draw) {
            draw_window_contents();
        }
        ImGui::End();
        return !to_close;
    }

private:
    plot::SwallowTaskData load_current_task() const
    {
        spdlog::stopwatch stopwatch;
        const auto path = data_dir / std::filesystem::path(tasks[current_task_index].npz_file.path);
        const auto ret = plot::SwallowTaskData::from_numpy(cnpy::npz_load(path.string()));
        spdlog::debug(
            "Data loaded from '{}' in {} ms", path.string(), stopwatch.elapsed_ms().count()
        );
        spdlog::debug(
            "{}: flow size = {}, audio size = {}", path.string(), ret.flow.size(), ret.audio.size()
        );
        return ret;
    }

    void set_task_index(std::size_t index)
    {
        current_task_index = index;
        task_plotter = SwallowTaskPlotter(load_current_task());
    }

    void draw_task_selector()
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
            set_task_index(new_index);
        }
    }

    void draw_window_contents()
    {
        draw_demo_windows();
        draw_task_selector();
        task_plotter.draw("##task_plotter");
        draw_debug_info();
    }

    void draw_debug_info() const
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("Mouse Position: [%.0f,%.0f]", io.MousePos.x, io.MousePos.y);
        ImGui::Text(
            "Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate
        );
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
    SwallowTaskPlotter task_plotter;

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
