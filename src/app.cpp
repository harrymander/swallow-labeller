#include "app.hpp"

#include "annotation-manager.hpp"
#include "labelling-task.hpp"

#include <filesystem>
#include <vector>

namespace recap::labeller::app {

App::App(
    std::vector<SwallowTaskInfo> swallow_tasks,
    SwallowAnnotationManager annotation_manager,
    std::filesystem::path data_dir
) :
    m_swallow_tasks(std::move(swallow_tasks)),
    m_annotation_manager(std::move(annotation_manager)),
    m_data_dir(std::move(data_dir))
{}

}; // namespace recap::labeller::app
