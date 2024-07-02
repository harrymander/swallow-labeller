#include "gui.hpp"

#include "annotation-manager.hpp"
#include "data.hpp"
#include "font.hpp"
#include "imgui-util.hpp"
#include "labelling-task.hpp"
#include "plotter.hpp"

#include <IconsFontAwesome6.h>
#include <cnpy.h>
#include <fmt/format.h>
#include <imgui.h>
#include <implot.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#include <algorithm>
#include <cstdlib>
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

    // Return new index
    std::size_t draw(const char *id, std::size_t index)
    {
        ScopedImID scoped_id(id);
        filter.Draw("##filter");
        ImGui::SameLine();

        // FIXME: currently this steps through all tasks, even if not displayed
        if (ImGui::ArrowButton("Prev task", ImGuiDir_Left)) {
            index = index ? index - 1 : tasks.size() - 1;
        }
        ImGui::SameLine();
        if (ImGui::ArrowButton("Next task", ImGuiDir_Right)) {
            index = (index + 1) % tasks.size();
        }

        const auto num_annotated = tasks.size() - annotated_start_index;
        ImGui::Text("Annotated: %zu out of %zu", num_annotated, tasks.size());
        if (ImGui::Button(only_show_annotated ? "Show all" : "Show annotated only")) {
            only_show_annotated = !only_show_annotated;
        }
        if (ImGui::BeginListBox("##listbox", {-1, -1})) {
            std::size_t i = only_show_annotated ? annotated_start_index : 0;
            for (; i < tasks.size(); i++) {
                const bool is_selected = (index == i);
                const char *str = tasks[i].c_str();
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

        return index;
    }

    [[nodiscard]] const SwallowTaskInfo& task_at(std::size_t index) const
    {
        return tasks[index].info();
    }

    void set_task_annotated(std::size_t& index)
    {
        if (task_annotated(index)) {
            return;
        }

        tasks.insert(
            tasks.begin() + static_cast<decltype(tasks)::difference_type>(annotated_start_index),
            {task_at(index), true}
        );
        annotated_start_index -= 1;
        tasks.erase(tasks.begin() + static_cast<decltype(tasks)::difference_type>(index));
        index = annotated_start_index;
    }

    void clear_task_annotated(std::size_t& index)
    {
        if (!task_annotated(index)) {
            return;
        }
        tasks.insert(tasks.begin(), {task_at(index), false});
        tasks.erase(tasks.begin() + static_cast<decltype(tasks)::difference_type>(index) + 1);
        annotated_start_index += 1;
        index = 0;
    }

private:
    [[nodiscard]] bool task_annotated(std::size_t index) const
    {
        return index >= annotated_start_index;
    }

    class TaskStrWrapper {
    public:
        TaskStrWrapper(SwallowTaskInfo info, bool annotated) :
            info_(std::move(info)),
            str_(fmt::format(
                "{}Subject #{}, {}\nRepeat #{}, swallow #{}",
                annotated ? ICON_FA_SQUARE_CHECK "  " : "",
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

    std::vector<TaskStrWrapper> tasks;
    std::size_t annotated_start_index;
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
        std::size_t& task_index,
        TaskList& task_list,
        AnnotationManager& annotation_mgr_
    ) :
        task_index(task_index),
        task(task_list.task_at(task_index)),
        task_list(task_list),
        annotation_mgr(annotation_mgr_)
    {
        const auto path = (data_dir / std::filesystem::path(task.npz_file.path)).make_preferred();
        path_str = path.string();
        const SwallowAnnotation *annotation_ = annotation_mgr.get_annotation(task.get_id());
        spdlog::debug(
            "Task at path '{}' (id={}) {} existing annotation",
            path,
            task.get_id(),
            annotation_ != nullptr ? "has" : "does not have"
        );
        new_annotation = annotation_ == nullptr;
        try {
            SwallowTaskData data = load_swallow_task_data(path);
            error_or_plotter.emplace<SwallowTaskPlotter>(
                data, annotation_ ? *annotation_ : SwallowAnnotation{}
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

    [[nodiscard]] const SwallowAnnotation *annotation() const
    {
        const auto *plotter = std::get_if<SwallowTaskPlotter>(&error_or_plotter);
        if (plotter && (!new_annotation || plotter->valid_annotation())) {
            return &plotter->annotation();
        }
        return nullptr;
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
                {-1, ImGui::GetContentRegionAvail().y - button_height - padding_y}
            ))
        {
            plotter.draw("##task_plot");
        }
        ImGui::EndChild();

        static const char *delete_button_str = "  " ICON_FA_TRASH_CAN "  ";
        float update_button_width = -1;
        if (!new_annotation) {
            const float del_button_width =
                ImGui::CalcTextSize(delete_button_str).x + ImGui::GetStyle().ItemInnerSpacing.x * 4;
            update_button_width = ImGui::GetContentRegionAvail().x - del_button_width;
        }

        ImGui::BeginDisabled(!plotter.valid_annotation());
        const char *button_str = new_annotation ? "Submit [Ctrl+Space]" : "Update [Ctrl+Space]";
        bool update_annotation = ImGui::Button(button_str, {update_button_width, button_height})
            || (!item_disabled() && ImGui::Shortcut(ImGuiKey_Space | ImGuiMod_Ctrl));
        if (update_annotation) {
            update_annotation = annotation_mgr.add_annotation(task.get_id(), plotter.annotation());
            task_list.set_task_annotated(task_index);
            new_annotation = false;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        if (!new_annotation) {
            if (ButtonRed(delete_button_str, {0, button_height})) {
                annotation_mgr.remove_annotation(task.get_id());
                task_list.clear_task_annotated(task_index);
                plotter.reset_annotation();
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

    std::size_t& task_index;
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

class ColorSchemeSelector {
public:
    void draw()
    {
        if (ImGui::Combo("Colour scheme", &style_id, "Dark\0Light\0Classic\0")) {
            switch (style_id) {
            case 0:
                ImGui::StyleColorsDark();
                break;
            case 1:
                ImGui::StyleColorsLight();
                break;
            case 2:
                ImGui::StyleColorsClassic();
                break;
            }
        }
    }

private:
    int style_id = 0;
};

std::optional<std::string> get_custom_ini_path()
{
    const char *const env = std::getenv("RECAP_LABELLER_IMGUI_INI_PATH");
    if (env) {
        std::filesystem::path path(env);
        if (std::filesystem::is_directory(path)) {
            spdlog::error("Invalid ImGui INI path: '{}' is a directory", path);
        } else if (!std::filesystem::is_directory(path.parent_path())) {
            spdlog::error(
                "Invalid ImGui INI path: parent directory '{}' does not exist or not a directory",
                path.parent_path()
            );
        } else {
            std::string path_str = path.make_preferred().string();
            spdlog::info("Custom ImGui INI path: {}", path_str);
            return path_str;
        }

        spdlog::warn("Ignoring RECAP_LABELLER_IMGUI_INI_PATH, using default ImGui INI path");
    } else {
        spdlog::debug("RECAP_LABELLER_IMGUI_INI_PATH not set, using default ImGui INI path");
    }

    return std::nullopt;
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
        auto custom_ini_path = get_custom_ini_path();
        if (custom_ini_path) {
            ini_path = std::move(*custom_ini_path);
            io.IniFilename = ini_path.c_str();
        }

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

    void stop()
    {
        if (annotation_unsaved()) {
            spdlog::info(
                "Application close requested, but there are unsaved changes. Prompting user."
            );
            gui_closing = true;
            next_task_index = task_index;
        } else {
            gui_ready_to_close = true;
        }
    }

    bool draw()
    {
        static const char *const UnsavedModalId =
            ICON_FA_TRIANGLE_EXCLAMATION "  Unsaved annotation";

        if (next_task_index.has_value() && !unsaved_modal_open) {
            ImGui::OpenPopup(UnsavedModalId);
            unsaved_modal_open = true;
        }

        ImGui::SetNextWindowPos(
            ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)
        );
        if (ImGui::BeginPopupModal(UnsavedModalId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            draw_unsaved_modal();
            ImGui::EndPopup();
        }

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
        return !gui_ready_to_close;
    }

    static void set_scaling_factor(float scaling_factor)
    {
        ImGui::GetStyle().ScaleAllSizes(scaling_factor);
    }

private:
    static constexpr ImGuiWindowFlags WindowFlags =
        (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

    void draw_unsaved_modal()
    {
        static const char *close_button_str = "Close without saving";
        const float button_width =
            ImGui::CalcTextSize(close_button_str).x + 2 * ImGui::GetStyle().ItemInnerSpacing.x;

        bool close_popup = false;
        ImGui::TextUnformatted("Task has unsaved changes!");

        if (ButtonRed(close_button_str, {button_width, 0})) {
            spdlog::info("Discarding changes");
            close_popup = true;
            if (*next_task_index != task_index) {
                set_task_index(*next_task_index);
            }
            gui_ready_to_close = gui_closing;
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", {button_width, 0})) {
            spdlog::debug("Cancel task close");
            close_popup = true;

            if (gui_closing) {
                spdlog::info("GUI close cancelled");
                gui_closing = false;
            }
        }
        ImGui::SetItemDefaultFocus();

        if (close_popup) {
            next_task_index.reset();
            unsaved_modal_open = false;
            ImGui::CloseCurrentPopup();
        }
    }

    void draw_menu_bar()
    {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Quit", "Alt+F4")) {
                spdlog::info("Quit requested from menu bar");
                stop();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            color_scheme_selector.draw();
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
                {ImGui::GetContentRegionAvail().x / 6, 0},
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

    [[nodiscard]] bool annotation_unsaved() const
    {
        const auto *annotation = task_view->annotation();
        if (annotation == nullptr) {
            return false;
        }
        return !annotation_mgr.annotation_saved(
            task_list.task_at(task_index).get_id(), *annotation
        );
    }

    void draw_sidebar()
    {
        const std::size_t new_index = task_list.draw("##tasklist", task_index);
        if (new_index != task_index) {
            if (annotation_unsaved()) {
                next_task_index = new_index;
            } else {
                set_task_index(new_index);
            }
        }
    }

    void set_task_index(std::size_t index)
    {
        task_index = index;
        task_view = load_current_task_view();
    }

    [[nodiscard]] std::unique_ptr<TaskView> load_current_task_view()
    {
        return std::make_unique<TaskView>(data_dir, task_index, task_list, annotation_mgr);
    }

    static void draw_debug_info()
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text(
            ICON_FA_GEAR
            "  Mouse Position: [%.0f,%.0f]. Application average: %.3f ms/frame (%.1f FPS).",
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

    bool gui_ready_to_close = false;
    bool gui_closing = false;
    bool show_implot_demo = false;
    bool show_imgui_demo = false;
    bool show_imgui_metrics = false;
    bool show_debug_info = false;
    bool unsaved_modal_open = false;
    ColorSchemeSelector color_scheme_selector;
    std::string ini_path;
    std::size_t task_index = 0;
    std::optional<std::size_t> next_task_index = std::nullopt;

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
