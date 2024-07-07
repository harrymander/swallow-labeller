#include "app.hpp"

#include <filesystem>
#include <vector>

namespace recap::labeller::app {

App::App(
    std::vector<task::SwallowTaskInfo> swallow_tasks,
    annotation_manager::AnnotationManager annotation_manager,
    std::filesystem::path data_dir
) :
    m_swallow_tasks(std::move(swallow_tasks)),
    m_annotation_manager(std::move(annotation_manager)),
    m_data_dir(std::move(data_dir))
{}

}; // namespace recap::labeller::app
