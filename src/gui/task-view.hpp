#ifndef RECAP_LABELLER_GUI_TASK_VIEW_HPP_INCLUDE
#define RECAP_LABELLER_GUI_TASK_VIEW_HPP_INCLUDE

#include "app/task-loader.hpp"
#include "models/annotation.hpp"
#include "models/task-info.hpp"

#include <functional>
#include <memory>

namespace recap::labeller::gui {

using SaveAnnotationCallback = std::function<void(const models::SwallowAnnotation&)>;

class TaskView {
public:
    virtual ~TaskView() = default;
    virtual void draw() = 0;

    virtual bool has_unsaved_changes() const { return false; };
};

std::unique_ptr<TaskView> load_task_view(
    app::TaskLoader& loader,
    const std::filesystem::path& data_dir,
    const models::SwallowTaskInfo& task,
    const models::SwallowAnnotation *existing_annotation,
    const SaveAnnotationCallback& save_annotation_callback
);

}; // namespace recap::labeller::gui

#endif // RECAP_LABELLER_GUI_TASK_VIEW_HPP_INCLUDE
