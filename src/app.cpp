#include "app.hpp"

#include "annotation-manager.hpp"
#include "labelling-task.hpp"

#include <filesystem>
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
        spdlog::error("Data file not found at path '{}'", m_data_path);
        m_state = SwallowLabellingTaskState::DataFileNotFound;
    } else if (m_annotation.has_value()) {
        m_state = SwallowLabellingTaskState::Annotated;
    } else {
        m_state = SwallowLabellingTaskState::Unannotated;
    }
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
        [](const auto& task) { return task.state() == SwallowLabellingTaskState::Annotated; }
    ))
{}

}; // namespace recap::labeller::app
