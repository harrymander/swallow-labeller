#ifndef RECAP_LABELLER_APP_INCLUDE_HPP
#define RECAP_LABELLER_APP_INCLUDE_HPP

#include "annotation-manager.hpp"
#include "annotation.hpp"
#include "labelling-task.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <optional>
#include <vector>

namespace recap::labeller::app {

enum class SwallowLabellingTaskState {
    Unannotated,
    Annotated,
    DataFileNotFound,
};

class SwallowLabellingTask {
public:
    SwallowLabellingTask(
        recap::labeller::SwallowTaskInfo task_info,
        const std::filesystem::path& data_dir,
        std::optional<SwallowAnnotation> annotation
    );

    [[nodiscard]] SwallowLabellingTaskState state() const { return m_state; }

    [[nodiscard]] const recap::labeller::SwallowTaskInfo& info() const { return m_info; }

    [[nodiscard]] const std::string& data_path() const { return m_data_path; }

private:
    SwallowLabellingTaskState m_state = SwallowLabellingTaskState::Unannotated;

    recap::labeller::SwallowTaskInfo m_info;
    std::optional<SwallowAnnotation> m_annotation;
    std::string m_data_path;
};

class App {
public:
    App(const std::vector<SwallowTaskInfo>& swallow_tasks,
        SwallowAnnotationManager annotation_manager,
        const std::filesystem::path& data_dir);

    [[nodiscard]] const std::vector<SwallowLabellingTask>& tasks() const { return m_swallow_tasks; }

    [[nodiscard]] std::size_t num_annotated_tasks() const { return m_num_annotated_tasks; }

    [[nodiscard]] std::size_t active_task_index() const { return m_active_task_index; }

    void set_active_task_index(std::size_t index)
    {
        if (index < m_swallow_tasks.size()) {
            spdlog::debug("Setting task index: {}", index);
            m_active_task_index = index;
        } else {
            spdlog::error("Invalid task index: {}; not changing!", index);
        }
    }

private:
    std::vector<SwallowLabellingTask> m_swallow_tasks;
    SwallowAnnotationManager m_annotation_manager;
    std::size_t m_num_annotated_tasks;

    std::size_t m_active_task_index = 0;
};

}; // namespace recap::labeller::app

#endif // RECAP_LABELLER_APP_INCLUDE_HPP
