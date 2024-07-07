#ifndef RECAP_LABELLER_APP_INCLUDE_HPP
#define RECAP_LABELLER_APP_INCLUDE_HPP

#include "annotation-manager.hpp"
#include "labelling-task.hpp"

#include <filesystem>
#include <vector>

namespace recap::labeller::app {

class App {
public:
    App(std::vector<task::SwallowTaskInfo> swallow_tasks,
        annotation_manager::AnnotationManager annotation_manager,
        std::filesystem::path data_dir);

    // TODO: remove these three functions. Just for getting to work with OldGui...
    [[nodiscard]] const std::vector<task::SwallowTaskInfo>& swallow_tasks() const
    {
        return m_swallow_tasks;
    }

    annotation_manager::AnnotationManager& annotation_manager() & { return m_annotation_manager; }

    [[nodiscard]] const std::filesystem::path& data_dir() const { return m_data_dir; }

private:
    std::vector<task::SwallowTaskInfo> m_swallow_tasks;
    annotation_manager::AnnotationManager m_annotation_manager;
    std::filesystem::path m_data_dir;
};

}; // namespace recap::labeller::app

#endif // RECAP_LABELLER_APP_INCLUDE_HPP
