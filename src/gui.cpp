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
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <variant>
#include <vector>

namespace recap::labeller::gui {

using labelling_task::SwallowLabellingTask;
using namespace recap::labeller::plotter;

namespace {

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

plot::SwallowTaskData load_swallow_task_data(const std::filesystem::path& path)
{
    spdlog::stopwatch stopwatch;
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("file does not exist");
    } else if (std::filesystem::is_directory(path)) {
        throw std::runtime_error("is a directory");
    }
    std::ifstream stream(path, std::ios::binary | std::ios::in);

    const auto ret = plot::SwallowTaskData::from_numpy(cnpy::npz_load(stream));
    spdlog::debug(
        "Data loaded from '{}' in {} ms:\n\t#flow samples: {}, #audio samples: {}",
        path.string(),
        stopwatch.elapsed_ms().count(),
        ret.flow.size(),
        ret.audio.size()
    );
    return ret;
}

class TaskView {
public:
    using Variant = std::variant<std::string, SwallowTaskPlotter>;

    explicit TaskView(const std::filesystem::path& path) : path_str(path.string())
    {
        try {
            error_or_plotter.emplace<SwallowTaskPlotter>(load_swallow_task_data(path));
        } catch (const std::exception& e) {
            const auto what = e.what();
            spdlog::error("Error loading task at path '{}': {}", path_str, what);
            error_or_plotter = what;
        }
    }

    void draw()
    {
        const std::string *error_str = std::get_if<std::string>(&error_or_plotter);
        if (error_str) {
            ImGui::Text("Error loading task at path %s: %s", path_str.c_str(), error_str->c_str());
        } else {
            ImGui::TextUnformatted(path_str.c_str());
            std::get<SwallowTaskPlotter>(error_or_plotter).draw("#task_plot");
        }
    }

private:
    std::string path_str;
    Variant error_or_plotter;
};

}; // namespace

class Gui::Impl {
public:
    Impl(const std::vector<SwallowLabellingTask>& tasks, const std::filesystem::path& data_dir) :
        data_dir(data_dir), tasks(tasks), task_view(load_current_task()), task_list(tasks)
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
                WindowFlags | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoResize
                    | ImGuiWindowFlags_NoBringToFrontOnFocus
            ))
        {
            if (ImGui::BeginMenuBar()) {
                draw_menu_bar();
                ImGui::EndMenuBar();
            }
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

    void draw_menu_bar()
    {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Quit", "Alt+F4")) {
                spdlog::info("Quit requested from menu bar");
                stop();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("ImGui demo...") && !show_imgui_demo) {
                spdlog::debug("Opening ImGui demo window");
                show_imgui_demo = true;
            }
            if (ImGui::MenuItem("Debug/metrics...") && !show_imgui_metrics) {
                spdlog::debug("Opening ImGui debug/metrics window");
                show_imgui_metrics = true;
            }
            if (ImGui::MenuItem("ImPlot demo...") && !show_implot_demo) {
                spdlog::debug("Opening ImPlot demo window");
                show_implot_demo = true;
            }
            ImGui::Checkbox("Show debug info", &show_debug_info);
            ImGui::EndMenu();
        }
    }

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
            task_view.draw();
            if (show_debug_info) {
                draw_debug_info();
            }
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

    TaskView load_current_task() const
    {
        auto path = data_dir / std::filesystem::path(tasks[current_task_index].npz_file.path);
        path.make_preferred();
        return TaskView(path);
    }

    void set_task_index(std::size_t index)
    {
        current_task_index = index;
        task_view = load_current_task();
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
        if (show_imgui_demo) {
            ImGui::ShowDemoWindow(&show_imgui_demo);
        }
        if (show_imgui_metrics) {
            ImGui::ShowMetricsWindow();
        }
        if (show_implot_demo) {
            ImPlot::ShowDemoWindow(&show_implot_demo);
        }
    }

    bool to_close = false;
    bool show_implot_demo = false;
    bool show_imgui_demo = false;
    bool show_imgui_metrics = false;
    bool show_debug_info = true;
    std::size_t current_task_index = 0;

    std::filesystem::path data_dir;
    std::vector<SwallowLabellingTask> tasks;
    TaskView task_view;
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
