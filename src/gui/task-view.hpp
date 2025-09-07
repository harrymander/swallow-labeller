#ifndef RECAP_LABELLER_GUI_TASK_VIEW_HPP_INCLUDE
#define RECAP_LABELLER_GUI_TASK_VIEW_HPP_INCLUDE

#include "app/task-loader.hpp"
#include "models/task-info.hpp"

#include <memory>

namespace recap::labeller::gui {

class TaskView {
public:
    virtual ~TaskView() = default;
    virtual void draw() = 0;
};

std::unique_ptr<TaskView> load_task_view(
    app::TaskLoader& loader,
    const std::filesystem::path& data_dir,
    const models::SwallowTaskInfo& task
);

}; // namespace recap::labeller::gui

#endif // RECAP_LABELLER_GUI_TASK_VIEW_HPP_INCLUDE
