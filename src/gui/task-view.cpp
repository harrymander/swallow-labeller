#include "gui/task-view.hpp"

#include "gui/icons.h"
#include "imgui.h"
#include "models/data.hpp"
#include "models/task-info.hpp"

#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <memory>
#include <utility>

namespace recap::labeller::gui {

namespace {

class LoadedTaskView : public TaskView {
public:
    LoadedTaskView(models::SwallowTaskData&& data, models::SwallowTaskInfo info) :
        m_data(std::move(data)), m_task_info(std::move(info))
    {}

    void draw() override
    {
        ImGui::TextWrapped("TODO: plot task %s", m_task_info.npz_file.path.c_str());
    }

private:
    models::SwallowTaskData m_data;
    models::SwallowTaskInfo m_task_info;
};

class TaskErrorView : public TaskView {
public:
    explicit TaskErrorView(std::filesystem::path path) : m_path(std::move(path)) {}

    void draw() override
    {
        ImGui::TextWrapped(
            ERR_ICON ICON_TEXT_SPACE "Error loading data for path %s", m_path.string().c_str()
        );
    }

private:
    std::filesystem::path m_path;
};

}; // namespace

std::unique_ptr<TaskView> load_task_view(
    app::TaskLoader& loader,
    const std::filesystem::path& data_dir,
    const models::SwallowTaskInfo& task
)
{
    std::filesystem::path path = data_dir / task.npz_file.path;
    spdlog::debug("Loading task data {}...", path);
    auto task_data = loader.load_task_data(path);
    if (task_data.has_value()) {
        return std::make_unique<LoadedTaskView>(std::move(*task_data), task);
    }

    return std::make_unique<TaskErrorView>(path);
}

}; // namespace recap::labeller::gui
