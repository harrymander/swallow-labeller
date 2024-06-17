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
#include <sstream>
#include <vector>

namespace recap::labeller::gui {

using labelling_task::SwallowLabellingTask;
using namespace recap::labeller::plotter;

inline const char *bool_string(bool val)
{
    return val ? "true" : "false";
}

class TaskSelectorList {
public:
    explicit TaskSelectorList(const std::vector<SwallowLabellingTask>& tasks)
    {
        for (const auto& task : tasks) {
            std::stringstream ss;
            ss << "Subject #" << task.subject << ", "
               << labelling_task::swallow_test_type_string(task.test_type) << "\nRepeat #"
               << task.repeatnum << ", swallow #" << task.swallownum;
            list_item_texts.push_back(ss.str());
        }
    }

    void draw(const char *id, std::size_t& index)
    {
        ImGui::PushID(id);
        filter.Draw("##filter");
        ImGui::SameLine();
        if (ImGui::ArrowButton("Prev task", ImGuiDir_Left)) {
            index = index ? index - 1 : list_item_texts.size() - 1;
        }
        ImGui::SameLine();
        if (ImGui::ArrowButton("Next task", ImGuiDir_Right)) {
            index = (index + 1) % list_item_texts.size();
        }

        if (ImGui::BeginListBox("##listbox", {-1, -1})) {
            for (std::size_t i = 0; i < list_item_texts.size(); i++) {
                const bool is_selected = (index == i);
                const char *str = list_item_texts[i].c_str();
                if (filter.PassFilter(str)) {
                    if (ImGui::Selectable(str, is_selected)) {
                        index = i;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::EndListBox();
        }
        ImGui::PopID();
    }

private:
    ImGuiTextFilter filter;
    std::vector<std::string> list_item_texts;
};

class Gui::Impl {
public:
    Impl(const std::vector<SwallowLabellingTask>& tasks, const std::filesystem::path& data_dir) :
        data_dir(data_dir), tasks(tasks), task_plotter(load_current_task()), task_list(tasks)
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
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::SetNextWindowPos({0, 0});
        if (ImGui::Begin(
                "##mainwindow",
                nullptr,
                WindowFlags | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus
            ))
        {
            draw_window_contents();
        }
        ImGui::End();
        return !to_close;
    }

    void set_scaling_factor(float scaling_factor)
    {
        ImGui::GetStyle().ScaleAllSizes(scaling_factor);
    }

private:
    static constexpr ImGuiWindowFlags WindowFlags =
        (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

    void draw_window_contents()
    {
        if (ImGui::BeginChild(
                "##sidebar",
                {ImGui::GetWindowSize().x / 5, 0},
                ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX,
                WindowFlags
            ))
        {
            draw_sidebar();
        }
        ImGui::EndChild();

        ImGui::SameLine();
        if (ImGui::BeginChild("##content", {0, 0}, ImGuiChildFlags_None, WindowFlags)) {
            draw_demo_windows();
            task_plotter.draw("##task_plotter");
            draw_debug_info();
        }
        ImGui::EndChild();
    }

    void draw_sidebar()
    {
        std::size_t new_index = current_task_index;
        task_list.draw("##tasklist", new_index);
        if (new_index != current_task_index) {
            set_task_index(new_index);
        }
    }

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

    bool to_close = false;
    bool show_implot_demo = false;
    bool show_imgui_demo = false;
    std::size_t current_task_index = 0;

    std::filesystem::path data_dir;
    std::vector<SwallowLabellingTask> tasks;
    SwallowTaskPlotter task_plotter;
    TaskSelectorList task_list;
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

void Gui::set_scaling_factor(float scaling_factor)
{
    pimpl->set_scaling_factor(scaling_factor);
}

}; // namespace recap::labeller::gui
