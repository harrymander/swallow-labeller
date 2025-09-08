#ifndef RECAP_LABELLER_GUI_TASK_LIST_HPP_INCLUDE
#define RECAP_LABELLER_GUI_TASK_LIST_HPP_INCLUDE

#include "app/annotation-store.hpp"
#include "app/task-loader.hpp"
#include "models/task-info.hpp"

#include <filesystem>
#include <memory>
#include <vector>

namespace recap::labeller::gui {

class TaskList {
public:
    TaskList(
        const std::vector<models::SwallowTaskInfo>& tasks,
        const SwallowAnnotationStore& annotation_store,
        app::TaskLoader& task_loader,
        const std::filesystem::path& data_dir
    );
    ~TaskList();
    TaskList(const TaskList&) = delete;
    TaskList& operator=(const TaskList&) = delete;
    TaskList(TaskList&&) = delete;
    TaskList& operator=(TaskList&&) = delete;

    // Return pointer to newly selected task or nullptr if no change
    [[nodiscard]] const models::SwallowTaskInfo *draw();

    const models::SwallowTaskInfo& currently_selected_task() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}; // namespace recap::labeller::gui

#endif // RECAP_LABELLER_GUI_TASK_LIST_HPP_INCLUDE
