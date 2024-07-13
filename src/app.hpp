#ifndef RECAP_LABELLER_APP_INCLUDE_HPP
#define RECAP_LABELLER_APP_INCLUDE_HPP

#include "annotation-manager.hpp"
#include "labelling-task.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <vector>

namespace recap::labeller::app {

class App {
public:
    App(std::vector<task::SwallowTaskInfo> swallow_tasks,
        annotation_manager::AnnotationManager annotation_manager,
        std::filesystem::path data_dir);

    [[nodiscard]] const std::vector<task::SwallowTaskInfo>& tasks_info() const
    {
        return m_swallow_tasks;
    }

    [[nodiscard]] std::size_t num_annotated_tasks() const { return 0; }

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
    std::vector<task::SwallowTaskInfo> m_swallow_tasks;
    annotation_manager::AnnotationManager m_annotation_manager;
    std::filesystem::path m_data_dir;

    std::size_t m_active_task_index = 0;
};

}; // namespace recap::labeller::app

#endif // RECAP_LABELLER_APP_INCLUDE_HPP
