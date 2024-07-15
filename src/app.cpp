#include "app.hpp"

#include "annotation-manager.hpp"
#include "data.hpp"
#include "labelling-task.hpp"

#include <fmt/core.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace recap::labeller::app {

namespace fs = std::filesystem;

namespace {

std::vector<SwallowLabellingTask> labelling_tasks(
    const std::vector<SwallowTaskInfo>& tasks_info,
    const SwallowAnnotationManager& annotation_manager,
    const fs::path& data_dir
)
{
    std::vector<SwallowLabellingTask> tasks;
    for (const auto& info : tasks_info) {
        const SwallowAnnotation *annotation = annotation_manager.get_annotation(info.get_id());
        tasks.emplace_back(
            info, data_dir, annotation ? std::make_optional(*annotation) : std::nullopt
        );
    }
    return tasks;
}

}; // namespace

SwallowLabellingTask::SwallowLabellingTask(
    recap::labeller::SwallowTaskInfo task_info,
    const fs::path& data_dir,
    std::optional<SwallowAnnotation> annotation
) :
    m_info(std::move(task_info)), m_annotation(std::move(annotation))
{
    fs::path path = (data_dir / fs::path(m_info.npz_file.path)).make_preferred();
    m_data_path = path.string();
    if (!fs::is_regular_file(path)) {
        std::string err = fmt::format("Data file not found at path '{}'", m_data_path);
        spdlog::error(err);
        m_error_msg = std::move(err);
    }
}

void SwallowLabellingTask::set_error_msg(std::string str)
{
    m_error_msg = std::move(str);
}

void SwallowLabellingTask::clear_error_msg()
{
    m_error_msg.reset();
}

App::App(
    const std::vector<SwallowTaskInfo>& swallow_tasks,
    SwallowAnnotationManager annotation_manager,
    const fs::path& data_dir
) :
    m_swallow_tasks(labelling_tasks(swallow_tasks, annotation_manager, data_dir)),
    m_annotation_manager(std::move(annotation_manager)),
    m_num_annotated_tasks(std::count_if(
        m_swallow_tasks.begin(),
        m_swallow_tasks.end(),
        [](const auto& task) { return task.is_annotated(); }
    ))
{
    load_active_task();
}

void App::set_active_task_index(std::size_t index)
{
    if (index < m_swallow_tasks.size()) {
        if (index != m_active_task_index) {
            spdlog::debug("Setting task index to {}", index);
            m_active_task_index = index;
            load_active_task();
        } else {
            spdlog::warn("Task index is already {}; not changing!", index);
        }
    } else {
        spdlog::error("Invalid task index: {}; not changing!", index);
    }
}

void App::reload_active_task()
{
    spdlog::info("Reloading active task...");
    load_active_task();
}

void App::load_active_task()
{
    SwallowLabellingTask& task = m_swallow_tasks[m_active_task_index];
    const auto path = fs::path(task.data_path());
    if (!fs::is_regular_file(path)) {
        task.set_error_msg("Data file not found");
        m_active_task = make_unique_active_task_variant<ActiveSwallowLabellingTaskErrorView>(task);
    } else {
        try {
            std::ifstream stream(task.data_path());
            auto data = SwallowTaskData::from_numpy(cnpy::npz_load(stream));
            spdlog::debug("Loaded data from {}", task.data_path());
            task.clear_error_msg();
            m_active_task =
                make_unique_active_task_variant<ActiveSwallowLabellingTaskView>(task, data);
        } catch (const std::exception& e) {
            spdlog::error("Error loading data file from {}: {}", task.data_path(), e.what());
            task.set_error_msg(fmt::format("Error loading data file: {}", e.what()));
            m_active_task =
                make_unique_active_task_variant<ActiveSwallowLabellingTaskErrorView>(task);
        }
    }

    m_new_active_task_observable.notify(*m_active_task);
}

ActiveSwallowLabellingTaskView::ActiveSwallowLabellingTaskView(
    SwallowLabellingTask& task, SwallowTaskData data
) :
    m_task(task), m_data(std::move(data))
{}

}; // namespace recap::labeller::app
