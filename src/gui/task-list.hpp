#ifndef RECAP_LABELLER_GUI_TASK_LIST_HPP_INCLUDE
#define RECAP_LABELLER_GUI_TASK_LIST_HPP_INCLUDE

#include "app/annotation-store.hpp"
#include "models/task-info.hpp"

#include <memory>
#include <vector>

namespace recap::labeller::gui {

class TaskList {
public:
    TaskList(
        const std::vector<models::SwallowTaskInfo>& tasks,
        const SwallowAnnotationStore& annotation_store
    );
    ~TaskList();
    TaskList(const TaskList&) = delete;
    TaskList& operator=(const TaskList&) = delete;
    TaskList(TaskList&&) = delete;
    TaskList& operator=(TaskList&&) = delete;

    void draw(const char *id);

private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}; // namespace recap::labeller::gui

#endif // RECAP_LABELLER_GUI_TASK_LIST_HPP_INCLUDE
