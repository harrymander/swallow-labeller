#ifndef RECAP_LABELLER_APP_INCLUDE_HPP
#define RECAP_LABELLER_APP_INCLUDE_HPP

#include "annotation-manager.hpp"
#include "labelling-task.hpp"

namespace recap::labeller::app {

class App {
public:
    App(std::vector<task::SwallowTaskInfo> swallow_tasks,
        annotation_manager::AnnotationManager annotation_manager);

private:
    std::vector<task::SwallowTaskInfo> m_swallow_tasks;
    annotation_manager::AnnotationManager m_annotation_manager;
};

}; // namespace recap::labeller::app

#endif // RECAP_LABELLER_APP_INCLUDE_HPP
