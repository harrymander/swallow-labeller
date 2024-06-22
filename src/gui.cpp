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

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <variant>
#include <vector>

namespace recap::labeller::gui {

using namespace labelling_task;
using plot::SwallowTaskData;
using namespace recap::labeller::plotter;

namespace {

using AnnotationsMap = std::map<std::string, SwallowAnnotation>;

AnnotationsMap make_annotations_map(const std::vector<SwallowAnnotation>& annotations)
{
    std::map<std::string, SwallowAnnotation> map;
    for (const auto& annot : annotations) {
        map[annot.id] = annot;
    }
    return map;
}

class TaskManager {
public:
    TaskManager(const std::vector<SwallowTaskInfo>& all_tasks, const AnnotationsMap& annotations)
    {
        std::vector<TaskStrWrapper> annotated;
        std::vector<TaskStrWrapper> unannotated;
        for (auto task : all_tasks) {
            const auto found_annotated = annotations.find(task.get_id());
            if (found_annotated != annotations.end()) {
                annotated.emplace_back(TaskStrWrapper(std::move(task), true));
            } else {
                unannotated.emplace_back(TaskStrWrapper(std::move(task), false));
            }
        }

        annotated_start_index = static_cast<decltype(annotated_start_index)>(unannotated.size());
        tasks = std::move(unannotated);
        tasks.insert(tasks.end(), annotated.begin(), annotated.end());
    }

    // Return true if task changed
    bool draw(const char *id)
    {
        ImGui::PushID(id);
        filter.Draw("##filter");
        ImGui::SameLine();
        auto new_index = index;

        // FIXME: currently this steps through all tasks, even if not displayed
        if (ImGui::ArrowButton("Prev task", ImGuiDir_Left)) {
            new_index = index ? index - 1 : static_cast<decltype(new_index)>(tasks.size()) - 1;
        }
        ImGui::SameLine();
        if (ImGui::ArrowButton("Next task", ImGuiDir_Right)) {
            new_index = (index + 1) % static_cast<decltype(new_index)>(tasks.size());
        }

        if (ImGui::Button(only_show_annotated ? "Show all" : "Show annotated only")) {
            only_show_annotated = !only_show_annotated;
        }
        if (ImGui::BeginListBox("##listbox", {-1, -1})) {
            decltype(index) i;
            decltype(index) end;
            if (only_show_annotated) {
                i = annotated_start_index;
                end = static_cast<decltype(index)>(tasks.size());
            } else {
                i = 0;
                end = annotated_start_index;
            }
            for (; i < end; i++) {
                const bool is_selected = (index == i);
                const char *str = tasks[i].c_str();
                if (filter.PassFilter(str)) {
                    if (ImGui::Selectable(str, is_selected)) {
                        new_index = i;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::EndListBox();
        }
        ImGui::PopID();
        bool changed = new_index != index;
        index = new_index;
        return changed;
    }

    [[nodiscard]] const SwallowTaskInfo& current_task() const { return tasks[index].info(); }

    [[nodiscard]] bool current_task_annotated() const { return index >= annotated_start_index; }

    // Moves task to front of annotated
    // TODO: return to original position?
    void set_current_task_annotated()
    {
        if (current_task_annotated()) {
            return;
        }

        tasks.insert(tasks.begin() + annotated_start_index, {current_task(), true});
        annotated_start_index -= 1;
        tasks.erase(tasks.begin() + index);
        index = annotated_start_index;
    }

    // Moves task to front
    // TODO: return to original position?
    void clear_current_task_annotated()
    {
        if (!current_task_annotated()) {
            return;
        }
        tasks.insert(tasks.begin(), {current_task(), false});
        tasks.erase(tasks.begin() + index + 1);
        annotated_start_index += 1;
        index = 0;
    }

private:
    class TaskStrWrapper {
    public:
        TaskStrWrapper(SwallowTaskInfo info, bool annotated) : info_(std::move(info))
        {
            std::stringstream ss;
            if (annotated) {
                ss << "[annotated] ";
            }
            ss << "Subject #" << info_.subject << ", " << swallow_test_type_string(info_.test_type)
               << "\nRepeat #" << info_.repeatnum << ", swallow #" << info_.swallownum;
            str_ = ss.str();
        }

        [[nodiscard]] const SwallowTaskInfo& info() const { return info_; }

        [[nodiscard]] const char *c_str() const { return str_.c_str(); }

    private:
        SwallowTaskInfo info_;
        std::string str_;
    };

    bool only_show_annotated = false;
    ImGuiTextFilter filter;
    std::vector<TaskStrWrapper>::difference_type index = 0;

    std::vector<TaskStrWrapper> tasks;
    decltype(index) annotated_start_index;
};

SwallowTaskData load_swallow_task_data(const std::filesystem::path& path)
{
    spdlog::stopwatch stopwatch;
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("file does not exist");
    }
    if (std::filesystem::is_directory(path)) {
        throw std::runtime_error("is a directory");
    }
    std::ifstream stream(path, std::ios::binary | std::ios::in);

    auto ret = SwallowTaskData::from_numpy(cnpy::npz_load(stream));
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
            const auto& what = e.what();
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

template <class T, class Rng> std::vector<T> shuffled_vector(std::vector<T> v, Rng& rng)
{
    std::shuffle(v.begin(), v.end(), rng);
    return v;
}

template <class T> std::vector<T> shuffled_vector(std::vector<T> v)
{
    static std::random_device rd;
    static std::default_random_engine rng(rd());
    return shuffled_vector(std::move(v), rng);
}

}; // namespace

class Gui::Impl {
public:
    Impl(
        std::vector<SwallowTaskInfo> tasks_,
        const std::vector<SwallowAnnotation>& annotations,
        std::filesystem::path data_dir,
        bool shuffle
    ) :
        data_dir(std::move(data_dir)),
        annotations(make_annotations_map(annotations)),
        task_manager(shuffle ? shuffled_vector(std::move(tasks_)) : tasks_, this->annotations),
        task_view(load_current_task())
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

    static void set_scaling_factor(float scaling_factor)
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

            // TEMPORARY: this is just for testing
            if (task_manager.current_task_annotated()) {
                if (ImGui::Button("Clear annotation")) {
                    task_manager.clear_current_task_annotated();
                }
            } else {
                if (ImGui::Button("Set annotation")) {
                    task_manager.set_current_task_annotated();
                }
            }

            task_view.draw();
            if (show_debug_info) {
                draw_debug_info();
            }
        }
        ImGui::EndChild();
    }

    void draw_sidebar()
    {
        if (task_manager.draw("##tasklist")) {
            task_view = load_current_task();
        }
    }

    [[nodiscard]] TaskView load_current_task() const
    {
        auto path = data_dir / std::filesystem::path(task_manager.current_task().npz_file.path);
        path.make_preferred();
        return TaskView(path);
    }

    static void draw_debug_info()
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

    std::filesystem::path data_dir;
    AnnotationsMap annotations;
    TaskManager task_manager;
    TaskView task_view;
};

Gui::Gui(
    std::vector<SwallowTaskInfo> tasks,
    const std::vector<SwallowAnnotation>& annotations,
    std::filesystem::path data_dir,
    bool shuffle
) :
    pimpl(std::make_unique<Impl>(std::move(tasks), annotations, std::move(data_dir), shuffle))
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
