#ifndef RECAP_LABELLER_GUI_TASK_LIST_HPP_INCLUDE
#define RECAP_LABELLER_GUI_TASK_LIST_HPP_INCLUDE

#include "app/annotation-store.hpp"
#include "app/task-loader.hpp"
#include "gui/task-filter.hpp"
#include "models/task-info.hpp"

#include <filesystem>
#include <vector>

namespace recap::labeller::gui {

class TaskList {
public:
    // Return index of the selected task (same as active_task_idx if unchanged).
    // active_task_idx must be a valid index into tasks.
    std::size_t draw(
        const std::vector<models::SwallowTaskInfo>& tasks,
        std::size_t active_task_idx,
        const SwallowAnnotationStore& annotation_store,
        app::TaskLoader& m_task_loader,
        const std::filesystem::path& data_dir
    );

private:
    TaskFilter m_filter;
};

}; // namespace recap::labeller::gui

#endif // RECAP_LABELLER_GUI_TASK_LIST_HPP_INCLUDE
