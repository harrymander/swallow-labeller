#include "gui.hpp"

#include "annotation-manager.hpp"
#include "data.hpp"
#include "font.hpp"
#include "imgui-util.hpp"
#include "labelling-task.hpp"
#include "plotter.hpp"

#include <cnpy.h>
#include <fmt/format.h>
#include <imgui.h>
#include <implot.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <stdexcept>
#include <variant>
#include <vector>

namespace recap::labeller::gui {

using namespace recap::labeller::annotation_manager;
using namespace recap::labeller::plotter;
using namespace recap::labeller::task;
using namespace imgui_util;
using recap::labeller::data::SwallowTaskData;

namespace {

class TaskList {
public:
    TaskList(const std::vector<SwallowTaskInfo>& all_tasks, const AnnotationManager& annotation_mgr)
    {
        std::vector<TaskStrWrapper> annotated;
        std::vector<TaskStrWrapper> unannotated;
        for (auto task : all_tasks) {
            if (annotation_mgr.get_annotation(task.get_id()) != nullptr) {
                annotated.emplace_back(std::move(task), true);
            } else {
                unannotated.emplace_back(std::move(task), false);
            }
        }

        annotated_start_index = static_cast<decltype(annotated_start_index)>(unannotated.size());
        tasks = std::move(unannotated);
        tasks.insert(tasks.end(), annotated.begin(), annotated.end());
    }

    // Return true if task changed
    bool draw(const char *id)
    {
        ScopedImID scoped_id(id);
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
            decltype(index) i = only_show_annotated ? annotated_start_index : 0;
            for (; i < static_cast<decltype(index)>(tasks.size()); i++) {
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

        bool changed = new_index != index;
        index = new_index;
        return changed;
    }

    [[nodiscard]] const SwallowTaskInfo& current_task() const { return tasks[index].info(); }

    [[nodiscard]] bool current_task_annotated() const { return index >= annotated_start_index; }

    // Moves task to front of annotated
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
        TaskStrWrapper(SwallowTaskInfo info, bool annotated) :
            info_(std::move(info)),
            str_(fmt::format(
                "{}Subject #{}, {}\nRepeat #{}, swallow #{}",
                annotated ? "[annotated] " : "",
                info_.subject,
                swallow_test_type_string(info_.test_type),
                info_.repeatnum,
                info_.swallownum
            ))
        {}

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
        path,
        stopwatch.elapsed_ms().count(),
        ret.flow.size(),
        ret.audio.size()
    );
    return ret;
}

class TaskView {
public:
    using Variant = std::variant<std::string, SwallowTaskPlotter>;

    explicit TaskView(
        const std::filesystem::path& data_dir,
        TaskList& task_list,
        AnnotationManager& annotation_mgr_
    ) :
        task(task_list.current_task()), task_list(task_list), annotation_mgr(annotation_mgr_)
    {
        const auto path = (data_dir / std::filesystem::path(task.npz_file.path)).make_preferred();
        path_str = path.string();
        const SwallowAnnotation *annotation = annotation_mgr.get_annotation(task.get_id());
        spdlog::debug(
            "Task at path '{}' (id={}) {} existing annotation",
            path,
            task.get_id(),
            annotation != nullptr ? "has" : "does not have"
        );
        new_annotation = annotation == nullptr;
        try {
            SwallowTaskData data = load_swallow_task_data(path);
            error_or_plotter.emplace<SwallowTaskPlotter>(
                data, annotation ? *annotation : SwallowAnnotation{}
            );
        } catch (const std::exception& e) {
            std::string error = fmt::format("Error loading task at path '{}': {}", path, e.what());
            spdlog::error(error);
            error_or_plotter = error;
        }
    }

    void draw()
    {
        const std::string *error_str = std::get_if<std::string>(&error_or_plotter);
        if (error_str) {
            ImGui::Text("Error: %s", error_str->c_str());
        } else {
            draw_plotter(std::get<SwallowTaskPlotter>(error_or_plotter));
        }
    }

private:
    void draw_plotter(SwallowTaskPlotter& plotter)
    {
        ImGui::TextUnformatted(path_str.c_str());
        constexpr float ButtonHeightFactor = 1.5;
        const float button_height = ButtonHeightFactor * ImGui::GetFrameHeight();
        const float padding_y = 2 * ImGui::GetStyle().FramePadding.y;
        if (ImGui::BeginChild(
                "##task_plot_container",
                {-1, ImGui::GetContentRegionAvail().y - button_height - padding_y},
                0,
                ImGuiWindowFlags_AlwaysAutoResize
            ))
        {
            plotter.draw("##task_plot");
        }
        ImGui::EndChild();

        static const char *delete_button_str = "Delete";
        float update_button_width = -1;
        if (!new_annotation) {
            const float del_button_width =
                ImGui::CalcTextSize(delete_button_str).x + ImGui::GetStyle().ItemInnerSpacing.x * 4;
            update_button_width = ImGui::GetContentRegionAvail().x - del_button_width;
        }

        ImGui::BeginDisabled(!plotter.valid_annotation());
        bool update_annotation = ImGui::Button(
            new_annotation ? "Submit" : "Update", {update_button_width, button_height}
        );
        if (update_annotation) {
            update_annotation = annotation_mgr.add_annotation(task.get_id(), plotter.annotation());
            task_list.set_current_task_annotated();
            new_annotation = false;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        if (!new_annotation) {
            if (ButtonRed(delete_button_str, {0, button_height})) {
                annotation_mgr.remove_annotation(task.get_id());
                task_list.clear_current_task_annotated();
                new_annotation = true;
                update_annotation = true;
            }
        }

        if (update_annotation) {
            try {
                annotation_mgr.sync_to_file();
            } catch (const std::runtime_error& e) {
                std::string error = fmt::format("Error updating annotation file: {}", e.what());
                spdlog::error(error);
                error_or_plotter.emplace<std::string>(error);
            }
        }
    }

    SwallowTaskInfo task;
    TaskList& task_list;
    AnnotationManager& annotation_mgr;
    std::string path_str;
    bool new_annotation;
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
        AnnotationManager& annotation_mgr,
        std::filesystem::path data_dir,
        bool shuffle
    ) :
        data_dir(std::move(data_dir)),
        annotation_mgr(annotation_mgr),
        task_list(shuffle ? shuffled_vector(std::move(tasks_)) : tasks_, this->annotation_mgr),
        task_view(load_current_task_view())
    {
        spdlog::debug("Setting up ImGui and ImPlot...");
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        font::setup_fonts();
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
            if (show_debug_info) {
                draw_debug_info();
            }
            task_view->draw();
        }
        ImGui::EndChild();
    }

    void draw_sidebar()
    {
        if (task_list.draw("##tasklist")) {
            task_view = load_current_task_view();
        }
    }

    [[nodiscard]] std::unique_ptr<TaskView> load_current_task_view()
    {
        return std::make_unique<TaskView>(data_dir, task_list, annotation_mgr);
    }

    static void draw_debug_info()
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text(
            "Mouse Position: [%.0f,%.0f]. Application average: %.3f ms/frame (%.1f FPS).",
            io.MousePos.x,
            io.MousePos.y,
            1000.0f / io.Framerate,
            io.Framerate
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
    bool show_debug_info = false;

    std::filesystem::path data_dir;
    AnnotationManager& annotation_mgr;
    TaskList task_list;
    std::unique_ptr<TaskView> task_view;
};

Gui::Gui(
    std::vector<SwallowTaskInfo> tasks,
    AnnotationManager& annotation_mgr,
    std::filesystem::path data_dir,
    bool shuffle
) :
    pimpl(std::make_unique<Impl>(std::move(tasks), annotation_mgr, std::move(data_dir), shuffle))
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
