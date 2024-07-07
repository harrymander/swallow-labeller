#include "app.hpp"

namespace recap::labeller::app {

App::App(
    std::vector<task::SwallowTaskInfo> swallow_tasks,
    annotation_manager::AnnotationManager annotation_manager
) :
    m_swallow_tasks(std::move(swallow_tasks)), m_annotation_manager(std::move(annotation_manager))
{}

}; // namespace recap::labeller::app
